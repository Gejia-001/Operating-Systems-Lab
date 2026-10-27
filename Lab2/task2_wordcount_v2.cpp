
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <string>

// 保存每个文件的统计信息
struct buf {
    const char *filename;
    int wc_count;
    bool success;
};

buf args[2];

// 判断是否为字母或数字
bool letter(int c) {
    return c != EOF && isalnum((unsigned char)c);
}

// 子线程：统计一个文件的单词数量
void *count_words(void *arg) {
    buf *ptr = (buf *)arg;
    FILE *fp = fopen(ptr->filename, "r");

    if (fp == NULL) {
        perror(ptr->filename);
        pthread_exit(&ptr->wc_count);
    }

    int c, prev = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (!letter(c) && letter(prev)) {
            ptr->wc_count++;
        }
        prev = c;
    }

    // 处理文件末尾没有分隔符的单词
    if (letter(prev)) {
        ptr->wc_count++;
    }

    fclose(fp);
    ptr->success = true;

    // 返回本线程的统计结果
    pthread_exit(&ptr->wc_count);
}

// 可视化：绘制横向条形图
void bar(const std::string& name, int value, int max) {
    int filled = (max > 0) ? value * 28 / max : 0;
    if (filled > 28) filled = 28;

    std::string s;
    for (int i = 0; i < 28; ++i) {
        s += (i < filled ? "█" : "░");
    }

    printf("  %s %s %d\n", name.c_str(), s.c_str(), value);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("用法: %s 文件1 文件2\n", argv[0]);
        return 1;
    }

    args[0] = {argv[1], 0, false};
    args[1] = {argv[2], 0, false};

    pthread_t t1, t2;

    // 创建两个线程
    if (pthread_create(&t1, NULL, count_words, &args[0]) != 0) {
        printf("线程1创建失败\n");
        return 1;
    }

    if (pthread_create(&t2, NULL, count_words, &args[1]) != 0) {
        printf("线程2创建失败\n");
        pthread_join(t1, NULL);
        return 1;
    }

    void *result1 = NULL;
    void *result2 = NULL;

    // 等待线程结束并接收统计结果
    pthread_join(t1, &result1);
    pthread_join(t2, &result2);

    if (!args[0].success || !args[1].success) {
        printf("文件读取失败\n");
        return 1;
    }

    int count1 = *(int *)result1;
    int count2 = *(int *)result2;

    // 主线程汇总结果
    int total = count1 + count2;

    // 输出可视化结果
    printf("\n===== 解法二：线程独立统计 =====\n\n");

    char pad1[64], pad2[64];
    snprintf(pad1, sizeof pad1, "%-12s", argv[1]);
    snprintf(pad2, sizeof pad2, "%-12s", argv[2]);

    bar(pad1, count1, total);
    bar(pad2, count2, total);
    bar("总计        ", total, total);

    printf("\n两个线程已完成，主线程汇总结果。\n");
    printf("总单词数：%d\n", total);

    return 0;
}
