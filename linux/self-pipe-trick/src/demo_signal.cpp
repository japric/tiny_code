// 用空闲管道读端占位模拟业务 fd,辅助线程 300ms 后给主线程发 SIGINT,
// 验证信号经管道唤醒 poll,且回调在主循环线程里执行.
#include "signal_pipe.hpp"
#include <poll.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <cstdio>

int main() {
    std::string error;
    bool running = true;

    SignalPipe signals;
    if (!signals.init(&error)) {
        fprintf(stderr, "init failed: %s\n", error.c_str());
        return 1;
    }

    int idle_fds[2];  // 占位用的空闲管道,无人写入
    if (::pipe(idle_fds) == -1) { perror("pipe"); return 1; }

    // 回调在主循环的 dispatch() 里执行,不受 async-signal-safe 限制
    auto on_stop = [&](int signo) {
        printf("[callback] signal %d handled in main-loop thread (tid=%lu)\n",
               signo, (unsigned long)pthread_self());
        running = false;
    };
    if (!signals.set_handler(SIGINT, on_stop)) { fprintf(stderr, "set_handler SIGINT failed\n"); return 1; }
    if (!signals.set_handler(SIGTERM, on_stop)) { fprintf(stderr, "set_handler SIGTERM failed\n"); return 1; }

    pthread_t main_tid = pthread_self();
    std::atomic<bool> exited{false};
    std::thread sender([&]() {
        ::usleep(300 * 1000);
        pthread_kill(main_tid, SIGINT);                       // 300ms 后发给主线程
        for (int i = 0; i < 200 && !exited.load(); ++i)       // 兜底: 2s 后还没退出再补 SIGTERM
            ::usleep(10 * 1000);
        if (!exited.load()) pthread_kill(main_tid, SIGTERM);
    });

    struct pollfd fds[2] = {
        {idle_fds[0], POLLIN, 0},       // 业务 fd 占位
        {signals.read_fd(), POLLIN, 0},
    };
    int iterations = 0, eintr = 0, dispatched_total = 0;
    while (running) {
        ++iterations;
        int n = ::poll(fds, 2, -1);
        if (n == -1) {
            if (errno == EINTR) { ++eintr; continue; }  // 唤醒字节已在管道里,重试不会丢信号
            break;
        }
        if (fds[1].revents & POLLIN) {
            int d = signals.dispatch();
            if (d == -1) break;  // 管道读取失败
            dispatched_total += d;
        }
    }
    exited.store(true);
    sender.join();
    printf("loop exited: iterations=%d (EINTR retries=%d), dispatched=%d\n",
           iterations, eintr, dispatched_total);

    ::close(idle_fds[0]);
    ::close(idle_fds[1]);
    return 0;
}
