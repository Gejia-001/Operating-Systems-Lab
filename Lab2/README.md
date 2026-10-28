# Lab2：线程与并发实验

本目录包含 Pthreads 线程创建、等待、竞态、信号量互斥和线程结果汇总示例。源码可在 macOS 与 Windows（MSYS2 UCRT64）编译。

## 目录结构

```text
Lab2/
├── README.md
├── .vscode/
│   └── tasks.json
├── essay_a.txt
├── essay_b.txt
├── task1_sum.cpp
├── task1_race.cpp
├── task2_wordcount.cpp
└── task2_wordcount_v2.cpp
```

这里不提交编译生成的可执行文件；运行 VS Code 任务或手动编译时会在本机生成。

## 编译环境

### macOS

安装 Xcode Command Line Tools 后可使用 Apple Clang：

```sh
xcode-select --install
clang++ --version
```

如果已经安装过命令行工具，第一条命令不需要重复运行。

### Windows

使用 **MSYS2 UCRT64** 提供的 MinGW-w64 GCC 和 winpthreads。不要使用 MSVC 编译这些源码，因为 MSVC 默认没有 `pthread.h`。

在 MSYS2 UCRT64 终端安装 GCC：

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc
```

安装后，将 `C:\\msys64\\ucrt64\\bin` 加入 Windows 的 `PATH`，然后重启 VS Code。这样 VS Code 的 PowerShell 任务能找到 `g++`，程序运行时也能找到 winpthreads 的 DLL。若 MSYS2 安装在别的位置，请将上面的路径换成实际安装路径。

## 手动编译和运行

先在 VS Code 中打开 `Lab2` 文件夹，或在终端切换到该目录。下面的相对路径都以 `Lab2` 为当前目录。

### macOS

```sh
clang++ -std=c++11 -O2 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum
./task1_sum 1000000

clang++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race
./task1_race
./task1_race --fix

clang++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount
./task2_wordcount essay_a.txt essay_b.txt

clang++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2
./task2_wordcount_v2 essay_a.txt essay_b.txt
```

### Windows PowerShell

确认 `g++` 已在 `PATH` 中，并在 `Lab2` 目录运行：

```powershell
g++ -std=c++11 -O2 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum.exe
.\\task1_sum.exe 1000000

g++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race.exe
.\\task1_race.exe
.\\task1_race.exe --fix

g++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount.exe
.\\task2_wordcount.exe essay_a.txt essay_b.txt

g++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2.exe
.\\task2_wordcount_v2.exe essay_a.txt essay_b.txt
```

编译参数 `-pthread` 用于启用 Pthreads 支持；`-std=c++11` 指定本实验使用的 C++ 标准。

## VS Code 任务

`.vscode/tasks.json` **不是 VS Code 必需文件**。没有它，仍可在终端手动编译；它的作用是保存本文件夹的快捷运行任务，让你从 VS Code 里选择运行，不必每次重输完整命令。当前配置保留五项：无锁竞态、加锁对照、子线程求和、解法一和解法二。

### 怎样生成

打开整个 `Lab2` 文件夹，而不是只打开某一个 `.cpp` 文件。在 VS Code 菜单选择 **Terminal → Configure Tasks… → Create tasks.json file from template → Others**，VS Code 会在 `.vscode/tasks.json` 写入模板。然后把实验所需的任务定义填进去。

本目录的 `tasks.json` 已经包含五个任务和 Windows PowerShell 命令，所以一般无需重新生成。要运行时选择 **Terminal → Run Task…**，再选任务名称。macOS 可用 **⇧⌘B** 运行默认的第一个任务；Windows 可用 **Ctrl+Shift+B**。

VS Code 的工作区任务配置说明：[Integrate with External Tools via Tasks](https://code.visualstudio.com/docs/debugtest/tasks)。

## 程序分别演示什么

- **`task1_sum.cpp`**：主线程调用 `pthread_create` 创建求和线程，再用 `pthread_join` 等待计算完成。
- **`task1_race.cpp`**：两个线程并行增加共享计数。默认模式把原子读取和写入分开，因此仍可能丢失更新；加上 `--fix` 后用 Pthread 互斥锁保护更新。
- **`task2_wordcount.cpp`（解法一）**：两个线程分别读文件，用 P 操作 `sem_wait` 和 V 操作 `sem_post` 保护共享总数。macOS 不支持这里需要的未命名信号量 `sem_init`，因此使用命名信号量 `sem_open`，结束时关闭并清理；Windows 的 winpthreads 分支使用 `sem_init`。
- **`task2_wordcount_v2.cpp`（解法二）**：两个线程各自统计一个文件，并把自己的计数地址作为返回值；主线程通过 `pthread_join` 接收结果后再汇总。

单词统计把字母或数字序列视为单词，标点和空白作为分隔符；文件末尾没有分隔符时也会统计最后一个词。

## 注意事项

1. 竞态示例的结果受线程调度影响；无锁时不保证每次都丢计数，加锁后应等于期望值。
2. 两个 essay 文件必须和程序在同一目录，或在运行命令中提供正确的文件路径；找不到文件时先检查 VS Code 是否打开了 `Lab2` 文件夹。
3. macOS 的信号量通过 `sem_open` 创建并在程序结束时清理，避免直接调用不受支持的 `sem_init`。
4. Windows 若提示找不到 `g++` 或 winpthreads DLL，检查 MSYS2 的 `ucrt64\\bin` 是否已加入 `PATH`，并重启 VS Code。
5. 条形图使用 `#` 和 `-` 字符，降低不同终端的字体宽度和编码差异。
