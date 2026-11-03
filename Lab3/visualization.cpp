#include "visualization.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

// 【库存可视化】# 代表已占用位置，. 代表空位，总长度固定 10。
std::string stockBar(int stock) {
    return "[" + std::string(stock, '#') +
           std::string(BUFFER_CAPACITY - stock, '.') + "]";
}

// 【工作量可视化】按照各线程完成的数量绘制文本条形图。
std::string workloadBar(int value, int maximum) {
    constexpr int width = 24;
    const int filled = (maximum == 0) ? 0 : (value * width / maximum);
    return "[" + std::string(filled, '#') +
           std::string(width - filled, '.') + "]";
}

} // namespace

// 显示实验参数和日志字段含义。
void TerminalVisualizer::showHeader(int target) const {
    std::cout << "\n========== 实验8：生产者-消费者 / P-V 线程同步 ==========\n"
              << "生产者: 3 | 消费者: 2 | 库存容量: 10 | 产品: " << target << "\n"
              << "生产间隔: 2ms | 消费间隔: 3ms\n"
              << "P=生产者, C=消费者 | #=已占用库存, .=空位\n"
              << "每条日志: 序号 / 时间 / 线程 ID / 产品 ID / 库存 / 总产消 / 等待时长\n"
              << "------------------------------------------------------------\n"
              << std::flush;
}

// 多线程并发调用：仅在这里用 C++ 互斥锁保护打印顺序。
// 这把锁不参与生产者-消费者同步，核心同步仍由 sem_wait/sem_post 实现。
void TerminalVisualizer::onEvent(const Event& event) {
    const std::lock_guard<std::mutex> lock(outputMutex_);
    pending_.emplace(event.sequence, event);
    while (true) {
        auto next = pending_.find(nextSequence_);
        if (next == pending_.end()) break;
        printEvent(next->second);
        pending_.erase(next);
        ++nextSequence_;
    }
    std::cout << std::flush;
}

// 终端实时日志：每次生产/消费都显示线程、产品编号与库存变化。
void TerminalVisualizer::printEvent(const Event& event) const {
    std::cout << "[" << std::setw(3) << std::setfill('0') << event.sequence
              << std::setfill(' ') << "] +" << std::fixed << std::setprecision(1)
              << std::setw(6) << event.elapsedMs << "ms  "
              << (event.isProduction ? 'P' : 'C') << event.workerNumber
              << "(tid=" << event.threadId << ") "
              << (event.isProduction ? "生产" : "消费")
              << " #" << std::setw(3) << std::setfill('0') << event.productId
              << std::setfill(' ')
              << "  库存 " << std::setw(2) << event.stockBefore
              << "->" << std::setw(2) << event.stockAfter << " "
              << stockBar(event.stockAfter)
              << "  总产/消=" << event.producedTotal << "/" << event.consumedTotal
              << "  等待=" << std::setprecision(2) << event.semaphoreWaitMs << "ms\n";
}

// 【汇总可视化】实验结束后统计每个线程工作量、库存占用与正确性。
void TerminalVisualizer::showSummary(const Report& report) const {
    std::cout << "\n===================== 实验结果 =====================\n"
              << "实际生产数量 : " << report.produced << " / " << report.target << "\n"
              << "实际消费数量 : " << report.consumed << " / " << report.target << "\n"
              << "最终库存数量 : " << report.finalStock << " / " << BUFFER_CAPACITY << "\n"
              << "库存峰值     : " << report.maxStock << " / " << BUFFER_CAPACITY
              << " " << stockBar(report.maxStock) << "\n"
              << std::fixed << std::setprecision(2)
              << "平均库存     : " << report.averageStock << " (按生产/消费事件采样)\n"
              << "总执行时间   : " << report.elapsedMs << " ms\n"
              << "平均吞吐量   : " << (report.produced * 1000.0 / report.elapsedMs)
              << " 件/秒 (含终端日志开销)\n"
              << "生产者等待空位: " << report.producerWaitMs << " ms (3线程累计)\n"
              << "消费者等待产品: " << report.consumerWaitMs << " ms (2线程累计)\n";

    // 五个线程的工作量图，能够直接比较负载分配情况。
    std::cout << "\n---------- 各线程工作量 ----------\n";
    const int biggestProducer = *std::max_element(report.producedBy.begin(), report.producedBy.end());
    const int biggestConsumer = *std::max_element(report.consumedBy.begin(), report.consumedBy.end());
    for (int i = 0; i < PRODUCER_COUNT; ++i) {
        std::cout << "生产者 P" << i + 1 << " "
                  << workloadBar(report.producedBy[i], biggestProducer)
                  << " " << report.producedBy[i] << " 件\n";
    }
    for (int i = 0; i < CONSUMER_COUNT; ++i) {
        std::cout << "消费者 C" << i + 1 << " "
                  << workloadBar(report.consumedBy[i], biggestConsumer)
                  << " " << report.consumedBy[i] << " 件\n";
    }

    // 三项验证分别判断：总量正确、产品ID无重复/丢失、库存没有越界。
    const bool countOK = (report.produced == report.target && report.consumed == report.target);
    const bool passed = countOK && report.idsAreCorrect && report.stockIsCorrect;
    std::cout << "\n---------- 正确性检查 ----------\n"
              << "[" << (countOK ? "PASS" : "FAIL") << "] 生产/消费总数符合目标\n"
              << "[" << (report.idsAreCorrect ? "PASS" : "FAIL") << "] 产品编号无重复、无遗漏\n"
              << "[" << (report.stockIsCorrect ? "PASS" : "FAIL") << "] 库存始终在 0~10，结束后库存为 0\n"
              << "最终结论: " << (passed ? "实验通过：线程同步与互斥正确。" : "实验失败：请排查同步逻辑。")
              << "\n====================================================\n";
}
