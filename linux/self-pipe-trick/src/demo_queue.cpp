// 生产者线程连续 post 100 个整数,主线程通过 poll 唤醒并调用 take() 批量取出,
// 检查消息不丢不乱序,同时统计唤醒合并效果 (wakeups <= 消息数).
#include "wakeup_queue.hpp"
#include <poll.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <thread>

int main() {
    WakeupQueue q;
    if (!q.init()) { fprintf(stderr, "queue init failed\n"); return 1; }

    constexpr int kTotal = 100;
    std::atomic<bool> producer_done{false};

    std::thread producer([&]() {
        for (int i = 0; i < kTotal; ++i) {
            if (!q.post(i)) { fprintf(stderr, "post(%d) failed\n", i); }
            if (i % 10 == 0) ::usleep(1000);   // 每 10 条停 1ms,其余连续发送以触发唤醒合并
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::vector<int> received;
    int wakeups = 0, takes = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    struct pollfd pfd = {q.read_fd(), POLLIN, 0};

    while ((int)received.size() < kTotal) {
        if (std::chrono::steady_clock::now() > deadline) {
            printf("TIMEOUT: only %zu/%d received\n", received.size(), kTotal);
            break;
        }
        int n = ::poll(&pfd, 1, 100);
        if (n == -1 && errno == EINTR) continue;
        if (n == -1) break;
        if (pfd.revents & POLLIN) ++wakeups;
        auto batch = q.take();
        if (!batch.empty()) ++takes;
        received.insert(received.end(), batch.begin(), batch.end());
    }
    producer.join();

    long long expected = 1LL * kTotal * (kTotal - 1) / 2;
    long long sum = std::accumulate(received.begin(), received.end(), 0LL);
    bool order_ok = (int)received.size() == kTotal;
    for (size_t i = 0; order_ok && i < received.size(); ++i)
        if (received[i] != (int)i) order_ok = false;

    printf("messages=%zu wakeups=%d takes=%d sum=%lld(expected %lld) order=%s\n",
           received.size(), wakeups, takes, sum, expected, order_ok ? "OK" : "BROKEN");
    return (order_ok && sum == expected) ? 0 : 1;
}
