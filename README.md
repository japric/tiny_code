# tiny_code

代码示例仓库,每个示例独立一个目录,路径按 `<topic>/<name>/` 组织.

## 用法

在仓库根目录构建全部示例(根 CMake 会自动发现子目录):

```console
$ cmake -S . -B build && cmake --build build -j
$ ctest --test-dir build --output-on-failure
$ ./build/linux/self-pipe-trick/demo_signal
$ ./build/linux/self-pipe-trick/demo_queue
```

也可以只构建单个示例:

```console
$ cmake -S linux/self-pipe-trick -B linux/self-pipe-trick/build && cmake --build linux/self-pipe-trick/build -j
```

新增示例目录后不用改根 `CMakeLists.txt`,重新 configure 就会自动加入构建.

## 约定

- 一个目录对应一个示例,目录名用小写短横线(如 `self-pipe-trick`).
- 单个 `main` 的短演示如果拆成多个小文件,用 `NN_name.cpp` 编号(`00_`/`01_`/...).
- 只收能直接编译运行的代码;草稿和半成品放 `_scratch/`(已加入 `.gitignore`).
- 默认用 `include/ + src/ + test/(可选) + CMakeLists.txt` 布局,根 `.gitignore` 已忽略 `build/`;只有两三个文件的简单演示也可以平铺,同一目录内不混用.

