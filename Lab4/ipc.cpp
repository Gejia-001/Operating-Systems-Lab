// 引入字符串、终端输入输出和 POSIX 进程与管道接口。
#include "ipc.h" // 引入本模块的函数声明。
#include <cerrno> // 使用错误码判断系统调用是否被信号打断。
#include <iostream> // 使用标准输入、输出和错误提示。
#include <string> // 使用字符串保存消息。
#include <sys/types.h> // 提供进程编号类型。
#include <sys/wait.h> // 提供等待子进程的接口。
#include <unistd.h> // 提供管道、进程及读写接口。

// 按行发送消息；循环处理可能出现的不完整写入。
static bool sendLine(int fd, const std::string& text) { // fd 是管道写端。
    std::string data = text + '\n'; // 用换行符标记一条消息的结束。
    size_t sent = 0; // 记录已经写入的字节数。
    while (sent < data.size()) { // 直到整条消息写入管道。
        ssize_t n = write(fd, data.data() + sent, data.size() - sent); // 写入剩余字节。
        if (n < 0 && errno == EINTR) continue; // 被信号打断时重新写入。
        if (n <= 0) return false; // 写入失败时通知调用者。
        sent += static_cast<size_t>(n); // 更新已经写入的字节数。
    } // 结束写入循环。
    return true; // 返回发送成功。
} // 结束发送函数。

// 按行接收消息；遇到换行符表示一条消息接收完成。
static bool receiveLine(int fd, std::string& text) { // fd 是管道读端。
    text.clear(); // 清空上一次接收的内容。
    char ch; // 保存当前读到的一个字节。
    while (true) { // 持续读取直到消息结束或管道关闭。
        ssize_t n = read(fd, &ch, 1); // 从管道读取一个字节。
        if (n < 0 && errno == EINTR) continue; // 被信号打断时重新读取。
        if (n <= 0) return false; // 管道关闭或读取失败时结束接收。
        if (ch == '\n') return true; // 遇到换行符时返回完整消息。
        text += ch; // 将当前字节追加到消息中。
    } // 结束读取循环。
} // 结束接收函数。

// 用方块演示消息在管道中的移动方向。
static void showMovement(bool forward) { // forward 表示消息是否从父进程发往子进程。
    if (!isatty(STDOUT_FILENO)) return; // 输出不是终端时跳过动画，避免产生控制字符。
    for (int step = 0; step <= 20; ++step) { // 分步更新方块的位置。
        int position = forward ? step : 20 - step; // 根据方向计算当前方块的位置。
        std::cout << "\r  父进程  [" << std::string(position, '-') << "●" // 绘制方块左边的管道。
                  << std::string(20 - position, '-') << "]  子进程" << std::flush; // 绘制方块右边的管道。
        usleep(60000); // 暂停片刻，使移动过程可见。
    } // 结束动画循环。
    std::cout << "\n"; // 动画结束后换行。
} // 结束动画函数。

// 子进程读取父进程消息，再通过另一条管道回复。
static int childProcess(int input, int output) { // input 为读端，output 为写端。
    std::string message; // 保存父进程发送的消息。
    while (receiveLine(input, message)) { // 等待并接收父进程的消息。
        if (!sendLine(output, "已收到")) return 1; // 通过返回管道发送回复。
    } // 父进程关闭管道后退出循环。
    close(input); // 关闭子进程的读端。
    close(output); // 关闭子进程的写端。
    return 0; // 返回子进程执行成功。
} // 结束子进程通信函数。

// 创建两条单向管道和子进程，让父子进程按顺序双向通信。
int runCommunication() { // 实验的主要执行函数。
    int toChild[2], toParent[2]; // 两条管道分别承载两个方向的数据。
    if (pipe(toChild) == -1) { perror("创建父到子管道失败"); return 1; } // 创建第一条管道。
    if (pipe(toParent) == -1) { perror("创建子到父管道失败"); close(toChild[0]); close(toChild[1]); return 1; } // 创建第二条管道。
    pid_t pid = fork(); // 创建子进程，父子进程各自持有管道描述符。
    if (pid == -1) { perror("创建子进程失败"); close(toChild[0]); close(toChild[1]); close(toParent[0]); close(toParent[1]); return 1; } // 创建失败时释放资源。
    if (pid == 0) { // 进入子进程分支。
        close(toChild[1]); // 子进程不使用父到子管道的写端。
        close(toParent[0]); // 子进程不使用子到父管道的读端。
        int result = childProcess(toChild[0], toParent[1]); // 接收消息并发送回复。
        _exit(result); // 子进程结束并返回执行状态。
    } // 结束子进程分支。
    close(toChild[0]); // 父进程不使用父到子管道的读端。
    close(toParent[1]); // 父进程不使用子到父管道的写端。
    std::cout << "\n========== 匿名管道双向通信 ==========\n\n" // 显示实验标题。
              << "  父进程 PID：" << getpid() << "\n" // 显示操作系统分配的父进程编号。
              << "  子进程 PID：" << pid << "\n\n" // 显示操作系统分配的子进程编号。
              << "  管道 1：父进程发送 → 子进程接收\n" // 显示管道一的数据方向。
              << "\n  父进程 → 写端 [1] → [ 管道缓冲区 ] → 读端 [0] → 子进程\n\n" // 展示管道一的逻辑结构。
              << "  管道 2：子进程发送 → 父进程接收\n" // 显示管道二的数据方向。
              << "\n  子进程 → 写端 [1] → [ 管道缓冲区 ] → 读端 [0] → 父进程\n\n" // 展示管道二的逻辑结构。
              << "  [0] 是读端， [1] 是写端；每条管道各有自己的缓冲区。\n\n" // 解释管道两端及缓冲区。
              << "  ● 表示消息，动画用于展示传输方向。\n" // 说明动画符号的含义。
              << "  输入消息并回车；输入 /quit 退出。\n"; // 显示操作方法。
    std::string message, reply; // 分别保存输入消息和子进程的回复。
    int result = 0; // 保存父进程执行状态。
    while (true) { // 每轮完成一次发送和回复。
        std::cout << "\n[父进程] 输入 > " << std::flush; // 提示用户输入消息。
        if (!std::getline(std::cin, message) || message == "/quit") break; // 输入结束或退出命令时停止。
        std::cout << "\n  [管道 1] 父进程发送：" << message << "\n" << std::flush; // 显示将要发送的消息。
        showMovement(true); // 演示父进程到子进程的方向。
        if (!sendLine(toChild[1], message)) { std::cerr << "发送失败\n"; result = 1; break; } // 将消息写入第一条管道。
        if (!receiveLine(toParent[0], reply)) { std::cerr << "接收失败\n"; result = 1; break; } // 等待子进程通过第二条管道回复。
        std::cout << "  子进程已接收消息，并发出回复。\n\n"; // 收到回复后确认子进程完成接收。
        std::cout << "  [管道 2] 子进程回复：" << reply << "\n"; // 显示通过管道实际收到的回复。
        showMovement(false); // 演示子进程到父进程的方向。
        std::cout << "  父进程已收到回复，本轮通信完成。\n"; // 显示本轮通信结果。
    } // 结束交互循环。
    close(toChild[1]); // 关闭写端，使子进程读到管道结束。
    close(toParent[0]); // 关闭父进程的读端。
    int status = 0; // 保存子进程退出状态。
    while (waitpid(pid, &status, 0) == -1) { if (errno != EINTR) { perror("等待子进程失败"); return 1; } } // 等待子进程并处理信号中断。
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) result = 1; // 检查子进程是否正常完成。
    std::cout << "\n通信结束，子进程已退出。\n"; // 显示结束提示。
    return result; // 返回整个实验的执行状态。
} // 结束通信模块。
