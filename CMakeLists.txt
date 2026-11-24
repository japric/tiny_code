# 自动发现所有 <topic>/<name>/CMakeLists.txt,新增示例不用改本文件
# 根构建: cmake -S . -B build && cmake --build build -j
# 单目录构建: cmake -S linux/self-pipe-trick -B linux/self-pipe-trick/build && cmake --build linux/self-pipe-trick/build -j
cmake_minimum_required(VERSION 3.10)
project(tiny_code CXX)

# 让 CMake 生成 compile_commands.json,供 clangd 做代码跳转/补全
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

file(GLOB _topics LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
list(SORT _topics)
set(_examples_found)
foreach(_topic IN LISTS _topics)
    # 跳过隐藏目录, _scratch 草稿目录, build 产物目录
    if(_topic MATCHES "^[._]" OR _topic STREQUAL "build" OR NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${_topic})
        continue()
    endif()
    file(GLOB _examples LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${_topic}/*)
    foreach(_example IN LISTS _examples)
        if(_example MATCHES "/[._]" OR NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${_example})
            continue()
        endif()
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_example}/CMakeLists.txt)
            list(APPEND _examples_found ${_example})
            message(STATUS "example: ${_example}")
            add_subdirectory(${_example})
        endif()
    endforeach()
endforeach()

if(NOT _examples_found)
    message(STATUS "no example directories found (expect <topic>/<name>/CMakeLists.txt)")
endif()
