// task2_wordcount_v2.cpp：每个 Pthreads 线程独立统计一个文件，通过线程返回值交给主线程汇总。
// macOS 编译：clang++ -std=c++11 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2
// Windows 编译：g++ -std=c++11 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2.exe（MSYS2 UCRT64）

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <pthread.h>
#include <string>

// 一个任务结构体保存文件名、线程统计结果和读取状态。
struct FileTask {
    const char* filename;
    int wordCount;
    bool success;
};

FileTask tasks[2];  // 主线程创建线程前填好任务，线程各自只更新自己的结构体。

// 只把字母和数字视为单词字符，并安全处理 cctype 的参数范围。
bool letter(int c) {
    return c != EOF && std::isalnum(static_cast<unsigned char>(c)) != 0;
}

// 子线程独立统计一个文件，并将结构体中计数地址作为 pthread_join 返回值。
void* countWords(void* arg) {
    FileTask* task = static_cast<FileTask*>(arg);
    FILE* file = std::fopen(task->filename, "r");
    if (file == nullptr) {
        std::perror(task->filename);
        return &task->wordCount;  // 仍返回有效地址；success=false 通知主线程本次失败。
    }

    int current;
    int previous = 0;
    while ((current = std::fgetc(file)) != EOF) {
        if (!letter(current) && letter(previous)) {
            ++task->wordCount;
        }
        previous = current;
    }
    if (letter(previous)) {
        ++task->wordCount;  // 文件末尾没有分隔符时补计最后一个词。
    }

    std::fclose(file);
    task->success = true;
    return &task->wordCount;  // 线程返回自己的统计结果，不访问共享总数。
}

// 输出按总量缩放的字符条形图，显示两个文件各自的贡献。
void bar(const std::string& name, int value, int maxValue) {
    int filled = maxValue > 0 ? value * 28 / maxValue : 0;
    if (filled > 28) filled = 28;

    std::string blocks;
    for (int i = 0; i < 28; ++i) {
        blocks += (i < filled ? "#" : "-");
    }
    std::printf("  %s %s %d\n", name.c_str(), blocks.c_str(), value);
}

// 主线程创建两个统计线程，join 取回指针后做汇总并展示结果。
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::printf("用法: %s 文件1 文件2\n", argv[0]);
        return 1;
    }

    tasks[0] = {argv[1], 0, false};
    tasks[1] = {argv[2], 0, false};
    pthread_t threads[2];

    if (pthread_create(&threads[0], nullptr, countWords, &tasks[0]) != 0) {
        std::printf("线程1创建失败\n");
        return 1;
    }
    if (pthread_create(&threads[1], nullptr, countWords, &tasks[1]) != 0) {
        std::printf("线程2创建失败\n");
        pthread_join(threads[0], nullptr);  // 创建第二个线程失败时也要回收第一个线程。
        return 1;
    }

    void* result1 = nullptr;
    void* result2 = nullptr;
    pthread_join(threads[0], &result1);  // join 等待线程结束，并取回线程返回的计数地址。
    pthread_join(threads[1], &result2);
    if (!tasks[0].success || !tasks[1].success || result1 == nullptr || result2 == nullptr) {
        std::printf("文件读取失败\n");
        return 1;
    }

    int count1 = *static_cast<int*>(result1);
    int count2 = *static_cast<int*>(result2);
    int total = count1 + count2;  // 只有主线程在这里汇总，因此不需要互斥锁。

    std::printf("\n===== 解法二：线程独立统计 =====\n\n");
    char pad1[64], pad2[64];
    std::snprintf(pad1, sizeof(pad1), "%-12s", argv[1]);
    std::snprintf(pad2, sizeof(pad2), "%-12s", argv[2]);
    bar(pad1, count1, total);
    bar(pad2, count2, total);
    bar("总计        ", total, total);
    std::printf("\n两个线程已完成，主线程汇总结果。\n");
    std::printf("总单词数：%d\n", total);
    return 0;
}
