#pragma once
#include <atomic>
#include <csignal>
#include <functional>
#include <string>
#include <sys/types.h>
#include <cstring>

class SignalPipe {
public:
    static constexpr int kMaxSignal = 128;
    using Handler = std::function<void(int)>;

    SignalPipe() = default;
    ~SignalPipe();
    SignalPipe(const SignalPipe&) = delete;
    SignalPipe& operator=(const SignalPipe&) = delete;

    bool init(std::string* error);
    bool set_handler(int signo, Handler handler);
    int dispatch();
    int read_fd() const { return read_fd_; }

    static void handle_signal(int signo, siginfo_t* info, void* ctx);

private:
    int read_fd_ = -1;
    int write_fd_ = -1;
    std::atomic<uint8_t> pending_[kMaxSignal] = {};
    Handler handlers_[kMaxSignal] = {};
    struct sigaction old_handlers_[kMaxSignal] = {};
    bool installed_[kMaxSignal] = {};
    static SignalPipe* instance_;
};

// 信号处理函数里会写 pending_, 必须保证无锁 (async-signal-safe)
static_assert(std::atomic<uint8_t>::is_always_lock_free,
              "pending_ must be lock-free for async-signal-safety");
