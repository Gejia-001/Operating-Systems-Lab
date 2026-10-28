# Lab2：从零开始运行

## 1. 安装 VS Code 和 C++ 编译器

先安装 [Visual Studio Code](https://code.visualstudio.com/download)，再在扩展商店安装 Microsoft 的 **C/C++** 扩展。扩展提供代码提示，编译器需要另外安装。

- **macOS：**打开“终端”，运行 `xcode-select --install` 安装 Apple 命令行工具。已经安装过可跳过。
- **Windows：**安装 [MSYS2](https://www.msys2.org/)，打开 **UCRT64** 终端，运行：

  ```sh
  pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
  ```

  安装后把 `C:\msys64\ucrt64\bin` 加入 Windows 的 `PATH`，再重启 VS Code。若安装目录不同，请使用自己的路径。此实验需要 Pthreads，Windows 请用 MSYS2 GCC，不要用 MSVC。

## 2. 下载并打开实验文件夹

在 [GitHub 仓库](https://github.com/Gejia-001/Operating-Systems-Lab)点 **Code → Download ZIP**，解压后，在 VS Code 中选择 **File → Open Folder…**，打开里面的 `Lab2` 文件夹。

目录中已经附带 VS Code 任务配置，不用自己创建：

```text
Lab2/
├── .vscode/tasks.json
├── essay_a.txt
├── essay_b.txt
├── task1_sum.cpp
├── task1_race.cpp
├── task2_wordcount.cpp
└── task2_wordcount_v2.cpp
```

## 3. 选择任务运行

在 VS Code 菜单点 **Terminal → Run Task…**，选择要运行的项目。每个任务都会先编译，再运行：

1. 竞态演示（默认任务）
2. 加锁对照
3. 子线程求和
4. 单词统计（信号量解法）
5. 单词统计（线程返回值解法）

快捷运行默认任务：macOS 按 **⇧⌘B**；Windows 按 **Ctrl+Shift+B**。

## 注意事项

- 必须打开整个 `Lab2` 文件夹；单独打开某个 `.cpp` 文件时，任务和文本文件路径可能无法正常工作。
- Windows 若提示找不到 `g++`，检查 `PATH` 是否包含 MSYS2 的 `ucrt64\bin`，然后重启 VS Code。
- 无锁竞态的结果每次可能不同；加锁版本应得到正确总数。
- 可执行文件由任务在本机生成，仓库里不放二进制文件。
