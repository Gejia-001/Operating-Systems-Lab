// task1_sum.cpp：由 Pthreads 子线程计算 1 到 n 的整数和。
// macOS 编译：clang++ -std=c++11 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum
// Windows 编译：g++ -std=c++11 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum.exe（MSYS2 UCRT64）

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

// 子线程通过这个全局变量保存最终结果，主线程在线程 join 后读取。
long long sum = 0;

// 子线程入口：从参数读取 n，计算总和，再把结果写入共享变量。
void* compute(void* arg) {
    long long n = *static_cast<long long*>(arg);
    long long localSum = 0;
    for (long long i = 1; i <= n; ++i) {
        localSum += i;  // 在线程本地累计，最后一次写入共享结果。
    }
    sum = localSum;
    return nullptr;     // 本题通过共享变量传回结果，join 保证读取时写入已完成。
}

// 主线程解析输入、创建子线程并等待它结束。
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::printf("用法: %s 一个整数\n", argv[0]);
        return 1;
    }

    long long n = std::atoll(argv[1]);  // 参数对象在 pthread_join 前一直有效。
    pthread_t worker;
    if (pthread_create(&worker, nullptr, compute, &n) != 0) {
        std::printf("子线程创建失败\n");
        return 1;
    }
    std::printf("主线程：子线程已启动，我来等它\n");

    if (pthread_join(worker, nullptr) != 0) {
        std::printf("等待子线程失败\n");
        return 1;
    }
    std::printf("1..%lld 的和 = %lld\n", n, sum);
    return 0;
}
