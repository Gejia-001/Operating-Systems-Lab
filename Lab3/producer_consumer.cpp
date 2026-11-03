#include "producer_consumer.h"

#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr int PRODUCE_DELAY_MS = 2;
constexpr int CONSUME_DELAY_MS = 3;

// 【信号量封装】使用 POSIX 命名信号量，兼容 macOS 与 Linux。
// 创建后立即删除名字：当前进程仍可使用它，异常退出时也不会残留名称。
class NamedSemaphore {
public:
    NamedSemaphore(const std::string& suffix, unsigned initialValue) {
        const std::string name = "/e8_" + std::to_string(getpid()) + "_" + suffix;
        sem_unlink(name.c_str()); // 清理同名的历史残留（正常运行不会存在）。
        value_ = sem_open(name.c_str(), O_CREAT | O_EXCL, 0600, initialValue);
        if (value_ == SEM_FAILED) {
            throw std::runtime_error("sem_open 失败: " + std::string(std::strerror(errno)));
        }
        sem_unlink(name.c_str());
    }

    ~NamedSemaphore() {
        if (value_ != SEM_FAILED) sem_close(value_);
    }

    NamedSemaphore(const NamedSemaphore&) = delete;
    NamedSemaphore& operator=(const NamedSemaphore&) = delete;
    sem_t* get() const { return value_; }

private:
    sem_t* value_ = SEM_FAILED;
};

// 【P 操作】申请资源；资源不足时线程在这里等待。
// 被系统信号中断时重新等待，不把 EINTR 当成普通错误。
void P(sem_t* semaphore) {
    while (sem_wait(semaphore) != 0) {
        if (errno == EINTR) continue;
        std::perror("sem_wait");
        std::abort();
    }
}

// 【V 操作】归还资源，必要时唤醒一个等待中的线程。
void V(sem_t* semaphore) {
    if (sem_post(semaphore) != 0) {
        std::perror("sem_post");
        std::abort();
    }
}

// 将 pthread_self() 转为可显示的字符串，兼容不同系统的 pthread_t 类型。
std::string currentThreadId() {
    std::ostringstream out;
    out << pthread_self();
    return out.str();
}

// 计算时间间隔（毫秒），用于事件时间线和信号量等待统计。
double milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 【共享数据】环形队列是真正的库存，生产者和消费者都访问它。
// empty/full 控制数量，mutex 保护队列、产品编号和各项统计数据。
struct SharedState {
    explicit SharedState(int total, const EventCallback& callback)
        : target(total), onEvent(callback),
          empty("empty", BUFFER_CAPACITY), full("full", 0), mutex("mutex", 1),
          producedVisits(total + 1, 0), consumedVisits(total + 1, 0) {}

    const int target;
    const EventCallback& onEvent;
    NamedSemaphore empty;
    NamedSemaphore full;
    NamedSemaphore mutex;

    std::array<int, BUFFER_CAPACITY> buffer{};
    int head = 0;                // 消费者从 head 位置取产品。
    int tail = 0;                // 生产者向 tail 位置放产品。
    int stock = 0;               // 当前库存数量。
    int nextProductId = 1;       // 【共享变量】下一个未使用的产品编号。
    bool stockAlwaysValid = true;
    bool producersFinished = false; // 主线程在全部生产者退出后修改它。
    long long stockSum = 0;      // 计算每次操作后的库存采样均值。
    int eventSequence = 0;
    Report report;
    std::vector<int> producedVisits;
    std::vector<int> consumedVisits;
    Clock::time_point started = Clock::now();
};

// 向仓库放入一个编号：必须在 mutex 临界区内调用。
void push(SharedState& state, int id) {
    state.buffer[state.tail] = id;
    state.tail = (state.tail + 1) % BUFFER_CAPACITY;
    ++state.stock;
}

// 从仓库取出最早放入的编号：必须在 mutex 临界区内调用（FIFO）。
int pop(SharedState& state) {
    const int id = state.buffer[state.head];
    state.head = (state.head + 1) % BUFFER_CAPACITY;
    --state.stock;
    return id;
}

struct WorkerArgs {
    SharedState* state;
    int index; // 从 0 开始，显示时加 1。
};

// ==================== 【重点一：生产者线程算法】 ====================
void* producerMain(void* rawArgs) {
    auto* args = static_cast<WorkerArgs*>(rawArgs);
    SharedState& state = *args->state;
    const std::string tid = currentThreadId();

    while (true) {
        // P(empty)：必须有空位才能继续；记录等待时长供可视化使用。
        const auto waitStarted = Clock::now();
        P(state.empty.get());
        const double waitMs = milliseconds(waitStarted, Clock::now());

        // P(mutex)：进入临界区，独占库存和 productID。
        P(state.mutex.get());
        if (state.nextProductId > state.target) {
            // 产品已生产完：还回刚申请的空位，再退出线程。
            V(state.mutex.get());
            V(state.empty.get());
            break;
        }

        // 【核心】分配唯一产品编号，放入 10 格的环形库存。
        const int id = state.nextProductId++;
        const int before = state.stock;
        push(state, id);
        ++state.report.produced;
        ++state.report.producedBy[args->index];
        ++state.producedVisits[id];
        state.report.maxStock = std::max(state.report.maxStock, state.stock);
        state.stockAlwaysValid &= (state.stock >= 0 && state.stock <= BUFFER_CAPACITY);
        state.stockSum += state.stock;
        state.report.producerWaitMs += waitMs;

        // 在临界区记录真实状态；可视化打印放到临界区外。
        const Event event{++state.eventSequence, milliseconds(state.started, Clock::now()),
                          true, args->index + 1, tid, id, before, state.stock,
                          state.report.produced, state.report.consumed, waitMs};

        // V(mutex) 释放共享数据；V(full) 告诉消费者有新产品。
        V(state.mutex.get());
        V(state.full.get());
        state.onEvent(event);
        std::this_thread::sleep_for(std::chrono::milliseconds(PRODUCE_DELAY_MS));
    }
    return nullptr;
}

// ==================== 【重点二：消费者线程算法】 ====================
void* consumerMain(void* rawArgs) {
    auto* args = static_cast<WorkerArgs*>(rawArgs);
    SharedState& state = *args->state;
    const std::string tid = currentThreadId();

    while (true) {
        // P(full)：库存没有产品时，消费者在这里等待。
        const auto waitStarted = Clock::now();
        P(state.full.get());
        const double waitMs = milliseconds(waitStarted, Clock::now());

        // P(mutex)：互斥访问共享库存，避免多个消费者取到同一产品。
        P(state.mutex.get());
        if (state.stock == 0 && state.producersFinished) {
            // 全部生产完毕且库存为零：这是主线程发送的结束通知。
            V(state.mutex.get());
            break;
        }

        const int before = state.stock;
        const int id = pop(state);

        // 【核心】取出产品并登记消费数量，用于检查是否丢失/重复。
        ++state.report.consumed;
        ++state.report.consumedBy[args->index];
        ++state.consumedVisits[id];
        state.stockAlwaysValid &= (state.stock >= 0 && state.stock <= BUFFER_CAPACITY);
        state.stockSum += state.stock;
        state.report.consumerWaitMs += waitMs;

        const Event event{++state.eventSequence, milliseconds(state.started, Clock::now()),
                          false, args->index + 1, tid, id, before, state.stock,
                          state.report.produced, state.report.consumed, waitMs};

        // V(mutex) 离开临界区；V(empty) 通知生产者有一个空位。
        V(state.mutex.get());
        V(state.empty.get());
        state.onEvent(event);
        std::this_thread::sleep_for(std::chrono::milliseconds(CONSUME_DELAY_MS));
    }
    return nullptr;
}

// 【线程创建】发生致命错误直接终止进程，避免部分线程永久等待。
// POSIX 命名信号量已在创建时 unlink，系统可自动回收。
void startThread(pthread_t& thread, void* (*function)(void*), WorkerArgs* args) {
    const int error = pthread_create(&thread, nullptr, function, args);
    if (error != 0) {
        std::fprintf(stderr, "pthread_create 失败: %s\n", std::strerror(error));
        std::exit(EXIT_FAILURE);
    }
}

} // namespace：以上实现细节只在核心模块内部使用。

// ==================== 【重点三：主线程协调与结果核验】 ====================
Report runExperiment(int totalProducts, const EventCallback& onEvent) {
    SharedState state(totalProducts, onEvent);
    std::array<pthread_t, CONSUMER_COUNT> consumers{};
    std::array<pthread_t, PRODUCER_COUNT> producers{};
    std::array<WorkerArgs, CONSUMER_COUNT> consumerArgs{};
    std::array<WorkerArgs, PRODUCER_COUNT> producerArgs{};
    state.started = Clock::now();

    // 先启动两个消费者，再启动三个生产者：共 5 个 pthread 线程。
    for (int i = 0; i < CONSUMER_COUNT; ++i) {
        consumerArgs[i] = {&state, i};
        startThread(consumers[i], consumerMain, &consumerArgs[i]);
    }
    for (int i = 0; i < PRODUCER_COUNT; ++i) {
        producerArgs[i] = {&state, i};
        startThread(producers[i], producerMain, &producerArgs[i]);
    }

    // pthread_join：等待所有生产者结束，保证不会再出现新产品。
    for (pthread_t thread : producers) pthread_join(thread, nullptr);

    // 【结束机制】标记没有新产品，再发送两个额外通知唤醒消费者。
    // 消费者优先取完真实库存，之后发现「库存=0 且生产结束」就退出。
    // 两次 V(full) 是结束通知，不会虚构库存产品。
    P(state.mutex.get());
    state.producersFinished = true;
    V(state.mutex.get());
    for (int i = 0; i < CONSUMER_COUNT; ++i) V(state.full.get());

    // pthread_join：等待所有消费者结束，才能安全读取最终共享统计。
    for (pthread_t thread : consumers) pthread_join(thread, nullptr);

    state.report.target = totalProducts;
    state.report.elapsedMs = milliseconds(state.started, Clock::now());
    state.report.finalStock = state.stock;
    state.report.averageStock = static_cast<double>(state.stockSum) /
                                (state.report.produced + state.report.consumed);
    state.report.stockIsCorrect = state.stockAlwaysValid && state.stock == 0;

    // 【结果核验】每个 1..N 的编号必须恰好生产一次并消费一次。
    state.report.idsAreCorrect = true;
    for (int id = 1; id <= totalProducts; ++id) {
        if (state.producedVisits[id] != 1 || state.consumedVisits[id] != 1) {
            state.report.idsAreCorrect = false;
            break;
        }
    }
    return state.report;
}
