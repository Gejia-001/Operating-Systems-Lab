#include "producer_consumer.h"
#include "visualization.h"

#include <iostream>
#include <string>
#include <exception>

// 【程序入口】只负责参数解析、启动实验和打印结果；算法在 core 文件中。
int main(int argc, char* argv[]) {
    int target = 100; // PPT 要求不少于 100 个产品，默认正好 100。

    // 可以从终端指定数量，如 ./experiment8 120。
    if (argc > 2) {
        std::cerr << "用法: " << argv[0] << " [产品数量(>=100)]\n";
        return 1;
    }
    if (argc == 2) {
        try {
            std::size_t parsed = 0;
            target = std::stoi(argv[1], &parsed);
            if (parsed != std::string(argv[1]).size() || target < 100 || target > 10000) {
                std::cerr << "产品数量应为 100~10000 的整数。\n";
                return 1;
            }
        } catch (const std::exception&) {
            std::cerr << "请输入有效的整数产品数量。\n";
            return 1;
        }
    }

    try {
        TerminalVisualizer visualizer;
        visualizer.showHeader(target);

        // 【核心与展示分离】线程算法生成 Event，展示模块负责终端打印。
        Report report = runExperiment(target, [&](const Event& event) {
            visualizer.onEvent(event);
        });

        // 所有生产/消费线程都已 join，主线程输出最终报告。
        visualizer.showSummary(report);
        return (report.idsAreCorrect && report.stockIsCorrect &&
                report.produced == target && report.consumed == target) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "实验启动失败: " << error.what() << '\n';
        return 1;
    }
}
