// task1_race.cpp：两个 Pthreads 线程同时累加共享计数器，比较竞态和互斥锁。
// macOS 编译：clang++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race
// Windows 编译：g++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race.exe（MSYS2 UCRT64）

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <string>

// 两个线程共享原子计数器；分开的原子读写仍会产生丢失更新，但不会引入 C++ 数据竞争。
std::atomic<int> counter(0);
pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;  // 加锁分支保护临界区。
bool useLock = false;
long long rounds = 10000000;

// 线程入口：按选择执行有锁或无锁的共享计数器更新。
void* work(void*) {
    for (long long i = 0; i < rounds; ++i) {
        if (useLock) {
            pthread_mutex_lock(&counterMutex);  // Pthread mutex 将读、加、写作为一个临界区。
            counter.fetch_add(1, std::memory_order_relaxed);
            pthread_mutex_unlock(&counterMutex);
        } else {
            int oldValue = counter.load(std::memory_order_relaxed);
            counter.store(oldValue + 1, std::memory_order_relaxed);  // 分开的原子读写间可插入另一线程更新。
        }
    }
    return nullptr;
}

// 把数值换算成固定宽度的字符条形图，方便对照期望值和实际值。
void bar(const char* name, long long value, long long maxValue) {
    int filled = maxValue > 0 ? static_cast<int>(value * 20 / maxValue) : 0;
    if (filled < 0) filled = 0;
    if (filled > 20) filled = 20;

    std::string blocks;
    for (int i = 0; i < 20; ++i) {
        blocks += (i < filled ? "#" : "-");  // 用 ASCII 图形避免不同终端的字符宽度差异。
    }
    std::printf("  %s  %s %lld\n", name, blocks.c_str(), value);
}

// 主线程读取运行模式，启动两个工作线程并在 join 后展示最终比较。
int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fix") == 0) useLock = true;
        else rounds = std::atoll(argv[i]);  // 可选参数用于缩短或加长实验。
    }

    long long expected = rounds * 2;
    std::printf("模式: %s   每个线程加 %lld 次   期望结果 %lld\n",
                useLock ? "加锁(正确)" : "不加锁(有竞态)", rounds, expected);

    auto start = std::chrono::steady_clock::now();  // 只计两线程运行区间的耗时。
    pthread_t first, second;
    if (pthread_create(&first, nullptr, work, nullptr) != 0) {
        std::printf("线程1创建失败\n");
        return 1;
    }
    if (pthread_create(&second, nullptr, work, nullptr) != 0) {
        std::printf("线程2创建失败\n");
        pthread_join(first, nullptr);
        return 1;
    }
    pthread_join(first, nullptr);   // 两次 join 后主线程才读取共享计数器。
    pthread_join(second, nullptr);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    int actual = counter.load(std::memory_order_relaxed);
    std::printf("\n结果 counter = %d   耗时 %lld ms\n\n", actual, static_cast<long long>(ms));
    bar("期望", expected, expected);
    bar("实际", actual, expected);
    bar("丢失", expected - actual, expected);
    std::printf("\n  %s\n", actual == expected ? "结果正确：一次也没丢"
                                                   : "结果偏小：这就是竞态，属预期现象");

    pthread_mutex_destroy(&counterMutex);  // 释放已不再使用的互斥锁资源。
    return 0;
}
