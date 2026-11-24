cmake_minimum_required(VERSION 3.10)
project(self-pipe-trick CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 封装 SignalPipe 实现,同时对外暴露 include/ 和 pthread 依赖
add_library(signal_pipe STATIC src/signal_pipe.cpp)
target_include_directories(signal_pipe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(signal_pipe PUBLIC pthread)

add_executable(demo_signal src/demo_signal.cpp)
target_link_libraries(demo_signal PRIVATE signal_pipe)

add_executable(demo_queue src/demo_queue.cpp)
target_link_libraries(demo_queue PRIVATE signal_pipe)

enable_testing()
add_executable(test_self_pipe test/test_self_pipe.cpp)
target_link_libraries(test_self_pipe PRIVATE signal_pipe)

add_test(NAME self_pipe_unit COMMAND test_self_pipe)
add_test(NAME self_pipe_demo_signal COMMAND demo_signal)
add_test(NAME self_pipe_demo_queue COMMAND demo_queue)
