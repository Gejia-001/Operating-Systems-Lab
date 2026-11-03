#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include "producer_consumer.h"

#include <map>
#include <mutex>

// 【纯展示模块】接收事件并在终端绘制日志/库存条；不修改核心库存。
class TerminalVisualizer {
public:
    void showHeader(int target) const;
    void onEvent(const Event& event);
    void showSummary(const Report& report) const;

private:
    void printEvent(const Event& event) const;

    std::mutex outputMutex_;       // 只保护终端输出，不是核心业务的库存锁。
    std::map<int, Event> pending_; // 线程回调可能乱序；按事件序号排序输出。
    int nextSequence_ = 1;
};

#endif
