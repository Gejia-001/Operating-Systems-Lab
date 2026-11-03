#ifndef PRODUCER_CONSUMER_H
#define PRODUCER_CONSUMER_H

#include <array>
#include <functional>
#include <string>

// 【实验固定条件】3 个生产者、2 个消费者、库存容量 10。
constexpr int PRODUCER_COUNT = 3;
constexpr int CONSUMER_COUNT = 2;
constexpr int BUFFER_CAPACITY = 10;

// 每发生一次真实的生产/消费操作，就生成一条记录交给可视化模块。
struct Event {
    int sequence;             // 事件序号（在共享库存的临界区内生成）
    double elapsedMs;         // 距离实验开始的时间（毫秒）
    bool isProduction;        // true=生产，false=消费
    int workerNumber;         // 生产者/消费者编号（从 1 开始）
    std::string threadId;     // pthread 自身的线程标识
    int productId;            // 本次生产/消费的产品编号
    int stockBefore;          // 操作前库存
    int stockAfter;           // 操作后库存
    int producedTotal;        // 当前累计生产数
    int consumedTotal;        // 当前累计消费数
    double semaphoreWaitMs;   // 本次等待空位/产品信号量的时间
};

// 实验完成后返回的统计结果，不包含绘图或终端输出代码。
struct Report {
    int target = 0;
    int produced = 0;
    int consumed = 0;
    int finalStock = 0;
    int maxStock = 0;
    double averageStock = 0.0;  // 每次生产/消费后的库存采样均值
    double elapsedMs = 0.0;
    double producerWaitMs = 0.0;
    double consumerWaitMs = 0.0;
    std::array<int, PRODUCER_COUNT> producedBy{};
    std::array<int, CONSUMER_COUNT> consumedBy{};
    bool idsAreCorrect = false; // 每个编号均生产一次、消费一次
    bool stockIsCorrect = false; // 库存始终在 [0, 10] 范围内
};

// 可视化模块注册此回调，核心算法只负责传递事件。
using EventCallback = std::function<void(const Event&)>;

// 启动生产者/消费者，等待全部结束，返回统计结果。
Report runExperiment(int totalProducts, const EventCallback& onEvent);

#endif
