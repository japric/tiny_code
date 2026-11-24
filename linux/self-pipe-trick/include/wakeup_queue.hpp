#pragma once
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

class WakeupQueue {
public:
    WakeupQueue() = default;
    ~WakeupQueue() {
        if (write_fd_ != -1) ::close(write_fd_);
        if (read_fd_ != -1) ::close(read_fd_);
    }
    WakeupQueue(const WakeupQueue&) = delete;
    WakeupQueue& operator=(const WakeupQueue&) = delete;

    bool init() {
        if (read_fd_ != -1) return false;
        int fds[2];
        if (::pipe2(fds, O_CLOEXEC | O_NONBLOCK) == -1) return false;
        read_fd_ = fds[0];
        write_fd_ = fds[1];
        return true;
    }
    int read_fd() const { return read_fd_; }

    bool post(int value) {
        bool need_write = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(value);
            if (!notified_) {
                notified_ = true;
                need_write = true;
            }
        }
        if (need_write) {
            char byte = 1;
            ssize_t written;
            do {
                written = ::write(write_fd_, &byte, 1);
            } while (written == -1 && errno == EINTR);
            if (written == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                // EAGAIN 表示管道缓冲区已满(里面已有未读数据),可直接忽略;
                // EBADF/EPIPE 等错误需复位 notified_ 并返回失败,防止后续生产者不再写管道
                std::lock_guard<std::mutex> lock(mutex_);
                notified_ = false;
                return false;
            }
        }
        return true;
    }

    std::vector<int> take() {
        char buf[256];
        for (;;) {
            ssize_t n = ::read(read_fd_, buf, sizeof(buf));
            if (n > 0) continue;
            if (n == -1 && errno == EINTR) continue;
            if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;  // 管道已空
            if (n == 0) break;            // EOF (写端已关闭,正常不会出现)
            break;                        // 其他读错误,放弃继续排空
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<int> batch;
        batch.swap(queue_);
        notified_ = false;
        return batch;
    }

private:
    int read_fd_ = -1;
    int write_fd_ = -1;
    std::mutex mutex_;
    std::vector<int> queue_;
    bool notified_ = false;
};
