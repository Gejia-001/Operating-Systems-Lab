// 引入进程、管道、文件及终端交互所需的接口。
#include "ipc.h" // 引入通信模块的声明。
#include <cerrno> // 判断系统调用是否被信号中断。
#include <csignal> // 使用信号结束无法继续运行的子进程。

#include <cstdlib> // 使用临时目录创建函数。
#include <iostream> // 使用终端输入输出。
#include <string> // 保存通信消息和管道路径。
#include <fcntl.h> // 使用 open 和文件打开标志。
#include <sys/stat.h> // 使用 mkfifo 创建命名管道。
#include <sys/types.h> // 使用进程编号类型。
#include <sys/wait.h> // 等待子进程结束。
#include <unistd.h> // 使用 pipe、fork、read 和 write。

// 将一行消息完整写入管道。
static bool sendLine(int fd, const std::string& text) { // fd 是写端。
    std::string data = text + '\n'; // 使用换行标记消息结束。
    size_t sent = 0; // 记录已经写入的字节数。
    while (sent < data.size()) { // 循环直到整条消息发送完成。
        ssize_t n = write(fd, data.data() + sent, data.size() - sent); // 写入剩余数据。
        if (n < 0 && errno == EINTR) continue; // 被信号打断时重试。
        if (n <= 0) return false; // 写入失败时返回错误。
        sent += static_cast<size_t>(n); // 更新已发送字节数。
    } // 结束写入循环。
    return true; // 表示发送成功。
} // 结束发送函数。

// 从管道读取一条以换行符结束的消息。
static bool receiveLine(int fd, std::string& text) { // fd 是读端。
    text.clear(); // 清空上一次接收的内容。
    char ch; // 存储当前读取的字节。
    while (true) { // 持续读取消息。
        ssize_t n = read(fd, &ch, 1); // 从管道读取一个字节。
        if (n < 0 && errno == EINTR) continue; // 被信号打断时重试。
        if (n <= 0) return false; // 管道关闭或读取失败时返回。
        if (ch == '\n') return true; // 读到结束符时返回完整消息。
        text += ch; // 将字节添加到消息中。
    } // 结束读取循环。
} // 结束接收函数。

// 在终端中用移动的方块演示通信方向。
static void showMovement(bool forward) { // forward 表示父进程向子进程发送。
    if (!isatty(STDOUT_FILENO)) return; // 重定向输出时不显示动画控制字符。
    for (int step = 0; step <= 20; ++step) { // 逐步更新方块位置。
        int pos = forward ? step : 20 - step; // 根据消息方向计算位置。
        std::cout << "\r  父进程  [" << std::string(pos, '-') << "●" // 输出方块左侧。
                  << std::string(20 - pos, '-') << "]  子进程" << std::flush; // 输出方块右侧。
        usleep(60000); // 暂停片刻以显示移动过程。
    } // 结束动画循环。
    std::cout << "\n"; // 动画结束后换行。
} // 结束动画函数。

// 子进程读取消息，向父进程发送确认回复。
static int childProcess(int input, int output) { // input 和 output 分别是读端与写端。
    std::string message; // 保存从父进程收到的消息。
    while (receiveLine(input, message)) { // 持续读取父进程发来的消息。
        if (!sendLine(output, "已收到：" + message)) return 1; // 回复实际收到的内容。
    } // 父进程关闭写端后结束循环。
    close(input); // 关闭子进程读端。
    close(output); // 关闭子进程写端。
    return 0; // 子进程正常结束。
} // 结束子进程函数。

// 父进程与子进程交换消息，并展示两条管道的逻辑结构。
static int parentProcess(int output, int input, pid_t pid, const std::string& kind) { // 接收两端描述符和管道类型。
    std::cout << "\n========== " << kind << "双向通信 ==========\n\n" // 显示当前选择的管道类型。
              << "  父进程 PID：" << getpid() << "    子进程 PID：" << pid << "\n\n" // 显示操作系统分配的进程编号。
              << "  管道 1：父进程 → 写端 → [缓冲区] → 读端 → 子进程\n\n" // 展示发送管道的结构。
              << "  管道 2：子进程 → 写端 → [缓冲区] → 读端 → 父进程\n\n" // 展示回复管道的结构。
              << "  输入 /quit 退出。\n"; // 显示退出方式。
    std::string message, reply; // 分别保存发送内容和回复。
    int result = 0; // 保存整个通信过程的状态。
    while (true) { // 每次循环完成一次双向通信。
        std::cout << "\n[父进程] 输入消息 > " << std::flush; // 提示用户输入消息。
        if (!std::getline(std::cin, message) || message == "/quit") break; // 遇到退出命令或输入结束时停止。
        std::cout << "\n  管道 1：父进程发送「" << message << "」\n"; // 显示即将发送的数据。
        showMovement(true); // 演示数据从父进程到子进程的方向。
        if (!sendLine(output, message)) { std::cerr << "发送失败\n"; result = 1; break; } // 写入发送管道。
        if (!receiveLine(input, reply)) { std::cerr << "接收失败\n"; result = 1; break; } // 从回复管道读取子进程回复。
        std::cout << "  子进程已收到消息。\n\n" // 显示子进程处理消息的结果。
                  << "  管道 2：子进程回复「" << reply << "」\n"; // 显示实际收到的回复。
        showMovement(false); // 演示回复从子进程到父进程的方向。
        std::cout << "  父进程已收到回复，本轮通信完成。\n"; // 显示本轮通信完成。
    } // 结束交互循环。
    close(output); // 关闭发送端，使子进程读到结束。
    close(input); // 关闭回复管道的读端。
    int status = 0; // 保存子进程退出状态。
    while (waitpid(pid, &status, 0) == -1) { if (errno != EINTR) { perror("等待子进程失败"); return 1; } } // 等待子进程并处理信号中断。
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) result = 1; // 检查子进程是否成功退出。
    std::cout << "\n通信结束，子进程已退出。\n"; // 显示程序结束状态。
    return result; // 返回通信结果。
} // 结束父进程函数。

// 使用两条匿名管道实现父子进程双向通信。
static int anonymousPipe() { // 创建匿名管道并运行通信。
    int forward[2], backward[2]; // 保存两个方向的读端和写端。
    if (pipe(forward) == -1) { perror("创建管道1失败"); return 1; } // 创建父到子的管道。
    if (pipe(backward) == -1) { perror("创建管道2失败"); close(forward[0]); close(forward[1]); return 1; } // 创建子到父的管道。
    pid_t pid = fork(); // 创建继承两条管道的子进程。
    if (pid == -1) { perror("创建子进程失败"); close(forward[0]); close(forward[1]); close(backward[0]); close(backward[1]); return 1; } // 创建失败时关闭全部端口。
    if (pid == 0) { // 进入子进程分支。
        close(forward[1]); close(backward[0]); // 关闭子进程不用的两个端口。
        _exit(childProcess(forward[0], backward[1])); // 子进程接收并回复。
    } // 结束子进程分支。
    close(forward[0]); close(backward[1]); // 父进程关闭不用的两个端口。
    return parentProcess(forward[1], backward[0], pid, "匿名管道"); // 父进程执行交互。
} // 结束匿名管道实验。

// 使用两条命名管道实现父子进程双向通信。
static int namedPipe() { // 创建临时 FIFO 并运行通信。
    char directory[] = "/tmp/ipc-lab-XXXXXX"; // 准备唯一临时目录的模板。
    if (!mkdtemp(directory)) { perror("创建临时目录失败"); return 1; } // 创建私有临时目录。
    std::string forward = std::string(directory) + "/to_child"; // 设置父到子的 FIFO 路径。
    std::string backward = std::string(directory) + "/to_parent"; // 设置子到父的 FIFO 路径。
    if (mkfifo(forward.c_str(), 0600) == -1) { perror("创建管道1失败"); rmdir(directory); return 1; } // 创建第一条命名管道。
    if (mkfifo(backward.c_str(), 0600) == -1) { perror("创建管道2失败"); unlink(forward.c_str()); rmdir(directory); return 1; } // 创建第二条命名管道。
    pid_t pid = fork(); // 创建子进程。
    if (pid == -1) { perror("创建子进程失败"); unlink(forward.c_str()); unlink(backward.c_str()); rmdir(directory); return 1; } // 创建失败时清理管道。
    if (pid == 0) { // 进入子进程分支。
        int input = open(forward.c_str(), O_RDONLY); // 打开父到子的命名管道读端。
        if (input == -1) { perror("打开子进程读端失败"); _exit(1); } // 检查读端是否打开成功。
        int output = open(backward.c_str(), O_WRONLY); // 打开子到父的命名管道写端。
        if (output == -1) { perror("打开子进程写端失败"); close(input); _exit(1); } // 检查写端是否打开成功。
        _exit(childProcess(input, output)); // 子进程接收消息并回复。
    } // 结束子进程分支。
    int output = open(forward.c_str(), O_WRONLY); // 打开父到子的命名管道写端。
    int input = output == -1 ? -1 : open(backward.c_str(), O_RDONLY); // 打开子到父的命名管道读端。
    int result = 1; // 默认记录失败状态。
    if (output == -1 || input == -1) { // 检查父进程是否打开管道成功。
        perror("打开父进程管道失败"); // 显示系统错误。
        if (output != -1) close(output); // 关闭已打开的写端。
        if (input != -1) close(input); // 关闭已打开的读端。
        kill(pid, SIGTERM); // 结束可能正在等待打开管道的子进程。
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR) {} // 回收子进程。
    } else { // 两条管道均已打开。
        result = parentProcess(output, input, pid, "命名管道（FIFO）"); // 运行父进程交互。
    } // 结束打开结果处理。
    unlink(forward.c_str()); // 删除第一条命名管道文件。
    unlink(backward.c_str()); // 删除第二条命名管道文件。
    rmdir(directory); // 删除用于保存管道的临时目录。
    return result; // 返回通信结果。
} // 结束命名管道实验。

// 让用户选择要运行的管道通信方法。
int runCommunication() { // 实验入口由 main 函数调用。
    std::cout << "\n选择双向通信方式：\n\n  1. 匿名管道\n  2. 命名管道（FIFO）\n\n请输入 1 或 2：" << std::flush; // 显示方法菜单。
    std::string choice; // 保存用户选择。
    if (!std::getline(std::cin, choice)) return 1; // 输入结束时退出。
    if (choice == "1") return anonymousPipe(); // 运行匿名管道实验。
    if (choice == "2") return namedPipe(); // 运行命名管道实验。
    std::cerr << "选择无效，请重新运行并输入 1 或 2。\n"; // 提示无效输入。
    return 1; // 返回选择失败。
} // 结束实验入口。
