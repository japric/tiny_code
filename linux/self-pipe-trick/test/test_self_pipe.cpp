// SignalPipe 与 WakeupQueue 的边界和异常路径测试.
#include "signal_pipe.hpp"
#include "wakeup_queue.hpp"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <thread>
#include <vector>

// 不用 assert(),避免 Release 构建定义了 NDEBUG 后检查被编译掉
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

static bool fd_readable(int fd, int timeout_ms = 0) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int n = ::poll(&pfd, 1, timeout_ms);
    return n == 1 && (pfd.revents & POLLIN);
}

static void dummy_sig_handler(int) {}

static void test_queue_init_and_uninit_post() {
    WakeupQueue q;
    CHECK(q.read_fd() == -1);
    CHECK(!q.post(1));  // 未 init 时 write(-1) 应返回失败

    CHECK(q.init());
    CHECK(q.read_fd() >= 0);
    CHECK(!q.init());   // 拒绝重复 init
}

static void test_queue_order_and_coalesce() {
    WakeupQueue q;
    CHECK(q.init());
    CHECK(!fd_readable(q.read_fd()));

    CHECK(q.post(10));
    CHECK(q.post(20));
    CHECK(q.post(30));
    CHECK(fd_readable(q.read_fd()));

    auto batch = q.take();
    CHECK(batch.size() == 3);
    CHECK(batch[0] == 10 && batch[1] == 20 && batch[2] == 30);
    CHECK(!fd_readable(q.read_fd()));

    // take() 复位 notified_ 后,再次 post 应重新触发唤醒
    CHECK(q.post(40));
    CHECK(fd_readable(q.read_fd()));
    batch = q.take();
    CHECK(batch.size() == 1 && batch[0] == 40);
}

static void test_queue_multithread() {
    WakeupQueue q;
    CHECK(q.init());

    constexpr int kPerThread = 500;
    constexpr int kTotal = kPerThread * 2;
    std::thread t1([&]() {
        for (int i = 0; i < kPerThread; ++i) CHECK(q.post(i));
    });
    std::thread t2([&]() {
        for (int i = kPerThread; i < kTotal; ++i) CHECK(q.post(i));
    });

    std::vector<int> seen(kTotal, 0);
    int count = 0;
    while (count < kTotal) {
        fd_readable(q.read_fd(), 100);
        for (int v : q.take()) {
            CHECK(v >= 0 && v < kTotal);
            CHECK(seen[v] == 0);
            seen[v] = 1;
            ++count;
        }
    }
    t1.join();
    t2.join();
    CHECK(count == kTotal);
}

static void test_queue_destructor_closes_fd() {
    int rfd = -1;
    {
        WakeupQueue q;
        CHECK(q.init());
        rfd = q.read_fd();
        CHECK(::fcntl(rfd, F_GETFD) != -1);
    }
    errno = 0;
    CHECK(::fcntl(rfd, F_GETFD) == -1 && errno == EBADF);
}

static void test_signal_init_and_invalid_signo() {
    std::string err;
    {
        SignalPipe sp;
        CHECK(sp.init(&err));
        CHECK(sp.read_fd() >= 0);

        // 重复 init 和同时创建第二个实例都应拒绝
        err.clear();
        CHECK(!sp.init(&err) && !err.empty());
        SignalPipe sp2;
        err.clear();
        CHECK(!sp2.init(&err) && !err.empty());

        CHECK(!sp.set_handler(0, [](int) {}));
        CHECK(!sp.set_handler(-1, [](int) {}));
        CHECK(!sp.set_handler(SignalPipe::kMaxSignal, [](int) {}));
    }
    // 析构清掉 instance_ 后,新实例应能正常 init
    SignalPipe sp3;
    CHECK(sp3.init(&err));
}

static void test_signal_dispatch_replace_and_restore() {
    struct sigaction prev_sa{};
    struct sigaction sentinel{};
    sentinel.sa_handler = &dummy_sig_handler;
    sigemptyset(&sentinel.sa_mask);
    CHECK(::sigaction(SIGUSR1, &sentinel, &prev_sa) == 0);

    {
        SignalPipe sp;
        std::string err;
        CHECK(sp.init(&err));

        int count1 = 0, count2 = 0, last_sig = 0;
        CHECK(sp.set_handler(SIGUSR1, [&](int sig) {
            ++count1;
            last_sig = sig;
        }));

        // 连续两次 raise 应合并为一次 dispatch
        CHECK(::raise(SIGUSR1) == 0);
        CHECK(::raise(SIGUSR1) == 0);
        CHECK(fd_readable(sp.read_fd(), 100));
        CHECK(sp.dispatch() == 1);
        CHECK(count1 == 1 && last_sig == SIGUSR1);
        CHECK(sp.dispatch() == 0);

        // 二次 set_handler 只替换业务回调,不覆盖保存的旧 sigaction
        CHECK(sp.set_handler(SIGUSR1, [&](int) { ++count2; }));
        CHECK(::raise(SIGUSR1) == 0);
        CHECK(fd_readable(sp.read_fd(), 100));
        CHECK(sp.dispatch() == 1);
        CHECK(count1 == 1 && count2 == 1);
    }

    struct sigaction after_sa{};
    CHECK(::sigaction(SIGUSR1, nullptr, &after_sa) == 0);
    CHECK(after_sa.sa_handler == &dummy_sig_handler);

    CHECK(::sigaction(SIGUSR1, &prev_sa, nullptr) == 0);
}

int main() {
    test_queue_init_and_uninit_post();
    test_queue_order_and_coalesce();
    test_queue_multithread();
    test_queue_destructor_closes_fd();
    test_signal_init_and_invalid_signo();
    test_signal_dispatch_replace_and_restore();
    printf("all 6 tests passed\n");
    return 0;
}
