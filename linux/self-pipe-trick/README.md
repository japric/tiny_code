# Self-Pipe Trick

用非阻塞管道把异步信号和跨线程通知接入 `poll` 主循环的最小示例.

## 文件

| 文件 | 说明 |
|---|---|
| `include/signal_pipe.hpp`, `src/signal_pipe.cpp` | `SignalPipe`: 把信号转成可读 fd,在主循环里派发回调 |
| `include/wakeup_queue.hpp` | `WakeupQueue`: 跨线程消息队列,通过管道唤醒 `poll` 并合并多次通知 |
| `src/demo_signal.cpp` | 演示用 `SignalPipe` 唤醒 `poll`,并确认回调在主循环线程里执行 |
| `src/demo_queue.cpp` | 生产者发 100 条消息,检查主线程收到的顺序与总和,同时统计唤醒次数 |
| `test/test_self_pipe.cpp` | `SignalPipe` 与 `WakeupQueue` 的边界与异常路径测试 |

## 构建运行

```console
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
$ ctest --test-dir build --output-on-failure
$ ./build/demo_signal
[callback] signal 2 handled in main-loop thread (tid=...)
loop exited: iterations=2 (EINTR retries=1), dispatched=1
$ ./build/demo_queue
messages=100 wakeups=<N> takes=<N> sum=4950(expected 4950) order=OK
```

100 条消息一般会合并成一二十次唤醒,具体次数随线程调度波动.
