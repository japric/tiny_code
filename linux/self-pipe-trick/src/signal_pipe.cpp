#include "signal_pipe.hpp"
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

SignalPipe* SignalPipe::instance_ = nullptr;

static std::string errno_message(const char* what) {
    return std::string(what) + ": " + std::strerror(errno);
}

bool SignalPipe::init(std::string* error) {
    if (read_fd_ != -1 || instance_ != nullptr) {
        if (error) *error = "already initialized";
        return false;
    }

    int fds[2];
    if (::pipe2(fds, O_CLOEXEC | O_NONBLOCK) == -1) {
        if (error) *error = errno_message("pipe2");
        return false;
    }

    read_fd_ = fds[0];
    write_fd_ = fds[1];
    instance_ = this;
    return true;
}

void SignalPipe::handle_signal(int signo, siginfo_t*, void*) {
    int saved_errno = errno;
    SignalPipe* self = instance_;
    if (self && signo > 0 && signo < kMaxSignal) {
        self->pending_[signo].store(1, std::memory_order_release);
    }
    if (self) {
        // 只写 1 字节用于唤醒 poll,信号编号已记在 pending_ 里
        char byte = 1;
        ssize_t written;
        do {
            written = ::write(self->write_fd_, &byte, 1);
        } while (written == -1 && errno == EINTR);
        // EAGAIN 表示管道缓冲区已满(已有未读数据),足够唤醒 poll,直接忽略
        (void)written;
    }
    errno = saved_errno;
}

bool SignalPipe::set_handler(int signo, Handler handler) {
    if (signo <= 0 || signo >= kMaxSignal) return false;

    handlers_[signo] = std::move(handler);
    if (installed_[signo]) return true;  // 已注册过 sigaction,只需替换回调

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &SignalPipe::handle_signal;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    // 首次注册时保存旧 sigaction,供析构恢复
    struct sigaction old_sa;
    if (::sigaction(signo, &sa, &old_sa) == -1) {
        handlers_[signo] = nullptr;
        return false;
    }
    old_handlers_[signo] = old_sa;
    installed_[signo] = true;
    return true;
}

int SignalPipe::dispatch() {
    char buf[256];
    for (;;) {
        ssize_t n = ::read(read_fd_, buf, sizeof(buf));
        if (n > 0) continue;          // 排空管道里的唤醒字节
        if (n == -1 && errno == EINTR) continue;
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;  // 管道已空
        if (n == 0) return -1;        // EOF: 写端意外关闭
        return -1;                    // 其他读错误,向上返回 -1
    }

    int dispatched = 0;
    for (int signo = 1; signo < kMaxSignal; ++signo) {
        if (pending_[signo].exchange(0, std::memory_order_acq_rel) && handlers_[signo]) {
            handlers_[signo](signo);
            ++dispatched;
        }
    }
    return dispatched;
}

// 先恢复旧 handler,避免关掉 fd 后再触发 handle_signal;
// 再先关写端后关读端,避免触发 SIGPIPE
SignalPipe::~SignalPipe() {
    for (int signo = 1; signo < kMaxSignal; ++signo) {
        if (installed_[signo]) {
            ::sigaction(signo, &old_handlers_[signo], nullptr);
        }
    }
    if (write_fd_ != -1) ::close(write_fd_);
    if (read_fd_ != -1) ::close(read_fd_);
    if (instance_ == this) instance_ = nullptr;
}
