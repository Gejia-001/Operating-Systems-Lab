// task2_wordcount.cpp：两个 Pthreads 线程统计两个文件，用 P/V 信号量保护共享总数。
// macOS 编译：clang++ -std=c++11 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount
// Windows 编译：g++ -std=c++11 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount.exe（MSYS2 UCRT64）

#include <cctype>
#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <cerrno>
#ifdef __APPLE__
#include <fcntl.h>
#include <unistd.h>
#endif

// sem_t 在 macOS 上通过命名信号量初始化；Windows winpthreads 使用未命名信号量。
#ifdef __APPLE__
sem_t* wordSemaphore = SEM_FAILED;
#else
sem_t wordSemaphore;
#endif

int total = 0;                 // 两个线程共同更新的单词总数，由信号量保护。
int perFile[2] = {0, 0};      // 每个线程只写自己的位置，主线程 join 后读取。

// 初始化 P/V 信号量：macOS 不支持 sem_init，因此改用 sem_open 创建进程内唯一命名信号量。
bool initSemaphore() {
#ifdef __APPLE__
    char name[64];
    std::snprintf(name, sizeof(name), "/wordcount_%ld", static_cast<long>(getpid()));
    sem_unlink(name);  // 清理同一进程号可能留下的旧名称，避免复用旧计数值。
    wordSemaphore = sem_open(name, O_CREAT | O_EXCL, 0600, 1);
    return wordSemaphore != SEM_FAILED;
#else
    return sem_init(&wordSemaphore, 0, 1) == 0;
#endif
}

// 释放信号量资源：分别关闭 macOS 命名信号量，或销毁 winpthreads 未命名信号量。
void destroySemaphore() {
#ifdef __APPLE__
    if (wordSemaphore != SEM_FAILED) {
        sem_close(wordSemaphore);
        char name[64];
        std::snprintf(name, sizeof(name), "/wordcount_%ld", static_cast<long>(getpid()));
        sem_unlink(name);
        wordSemaphore = SEM_FAILED;
    }
#else
    sem_destroy(&wordSemaphore);
#endif
}

// 等待信号量就是 P 操作；遇到中断时继续等待，确保进入临界区前成功。
void P() {
    while (sem_wait(
#ifdef __APPLE__
        wordSemaphore
#else
        &wordSemaphore
#endif
    ) == -1 && errno == EINTR) {
    }
}

// 释放信号量就是 V 操作，允许另一个线程进入临界区。
void V() {
    sem_post(
#ifdef __APPLE__
        wordSemaphore
#else
        &wordSemaphore
#endif
    );
}

// 只把字母和数字算作单词内容；转换为 unsigned char 避免 cctype 的未定义输入。
bool letter(int c) {
    return c != EOF && std::isalnum(static_cast<unsigned char>(c)) != 0;
}

struct FileTask {
    const char* filename;
    int index;
};

// 统计线程的实际入口：独立累计本文件数量，并通过信号量安全累加共享总数。
void* countFile(void* arg) {
    FileTask* task = static_cast<FileTask*>(arg);
    FILE* file = std::fopen(task->filename, "r");
    if (file == nullptr) {
        std::printf("打不开 %s\n", task->filename);
        return nullptr;
    }

    int current;
    int previous = 0;
    int localCount = 0;  // 本地计数只由当前线程访问，不需要信号量保护。
    while ((current = std::fgetc(file)) != EOF) {
        if (!letter(current) && letter(previous)) {
            P();          // P：进入共享总数的临界区。
            ++total;
            V();          // V：离开临界区，释放信号量。
            ++localCount;
        }
        previous = current;
    }
    if (letter(previous)) {  // 文件末尾没有分隔符时，把最后一个词补计入。
        P();
        ++total;
        V();
        ++localCount;
    }

    std::fclose(file);
    perFile[task->index] = localCount;  // 不同线程写不同下标，join 后主线程统一读取。
    return nullptr;
}

// 画横向字符条形图，按总单词数缩放。
void bar(const std::string& name, int value, int maxValue) {
    int filled = maxValue > 0 ? value * 28 / maxValue : 0;
    if (filled > 28) filled = 28;

    std::string blocks;
    for (int i = 0; i < 28; ++i) {
        blocks += (i < filled ? "#" : "-");
    }
    std::printf("  %s %s %d\n", name.c_str(), blocks.c_str(), value);
}

// 主线程初始化信号量、创建两个文件统计线程并在 join 后输出总数和图表。
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::printf("用法: %s 文件1 文件2\n", argv[0]);
        return 1;
    }
    if (!initSemaphore()) {
        std::perror("信号量初始化失败");
        return 1;
    }

    FileTask tasks[2] = {{argv[1], 0}, {argv[2], 1}};
    pthread_t threads[2];
    if (pthread_create(&threads[0], nullptr, countFile, &tasks[0]) != 0) {
        std::printf("线程1创建失败\n");
        destroySemaphore();
        return 1;
    }
    if (pthread_create(&threads[1], nullptr, countFile, &tasks[1]) != 0) {
        std::printf("线程2创建失败\n");
        pthread_join(threads[0], nullptr);
        destroySemaphore();
        return 1;
    }
    pthread_join(threads[0], nullptr);  // 等待两个文件的统计线程全部结束。
    pthread_join(threads[1], nullptr);

    char pad1[64], pad2[64];
    std::snprintf(pad1, sizeof(pad1), "%-12s", argv[1]);
    std::snprintf(pad2, sizeof(pad2), "%-12s", argv[2]);
    std::printf("\n===== 解法一：信号量 P/V 互斥 =====\n\n");
    bar(pad1, perFile[0], total);
    bar(pad2, perFile[1], total);
    bar("总计        ", total, total);

    destroySemaphore();  // 所有线程结束后才释放信号量。
    return 0;
}
