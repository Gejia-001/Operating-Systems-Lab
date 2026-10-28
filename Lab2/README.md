# Lab2：复制命令运行

## 1. 安装环境（只需一次）

macOS：先安装 [VS Code](https://code.visualstudio.com/download)，再打开“终端”运行：

```sh
xcode-select --install
```

Windows PowerShell（安装 VS Code、Git、MSYS2）：

```powershell
winget install --id Microsoft.VisualStudioCode --exact
winget install --id Git.Git --exact
winget install --id MSYS2.MSYS2 --exact
```

安装完成后，关闭并重新打开 PowerShell，再用命令打开 **MSYS2 UCRT64**：

```powershell
& "C:\msys64\ucrt64.exe"
```

在新开的 UCRT64 窗口运行：

```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```

## 2. 下载项目并用 VS Code 打开

macOS 终端：

```sh
git clone https://github.com/Gejia-001/Operating-Systems-Lab.git
cd Operating-Systems-Lab/Lab2
open -a "Visual Studio Code" .
```

Windows PowerShell：

```powershell
git clone https://github.com/Gejia-001/Operating-Systems-Lab.git
cd Operating-Systems-Lab\Lab2
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
code .
```

## 3. 在 VS Code 终端编译并运行

先确认终端当前目录是 `Lab2`。

macOS（Apple Clang）：

```sh
clang++ -std=c++11 -O2 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum && ./task1_sum 1000000
clang++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race && ./task1_race && ./task1_race --fix
clang++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount && ./task2_wordcount essay_a.txt essay_b.txt
clang++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2 && ./task2_wordcount_v2 essay_a.txt essay_b.txt
```

Windows PowerShell（MSYS2 GCC）：

```powershell
g++ -std=c++11 -O2 -Wall -Wextra -pthread task1_sum.cpp -o task1_sum.exe; if ($LASTEXITCODE -eq 0) { .\task1_sum.exe 1000000 }
g++ -std=c++11 -O2 -Wall -Wextra -pthread task1_race.cpp -o task1_race.exe; if ($LASTEXITCODE -eq 0) { .\task1_race.exe; .\task1_race.exe --fix }
g++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount.cpp -o task2_wordcount.exe; if ($LASTEXITCODE -eq 0) { .\task2_wordcount.exe essay_a.txt essay_b.txt }
g++ -std=c++11 -O2 -Wall -Wextra -pthread task2_wordcount_v2.cpp -o task2_wordcount_v2.exe; if ($LASTEXITCODE -eq 0) { .\task2_wordcount_v2.exe essay_a.txt essay_b.txt }
```

## 也可以点菜单运行

`.vscode/tasks.json` 已经配置好，不用生成。选择 **Terminal → Run Task…**，再点“运行 1–5”中的一项；任务会自动编译并运行。

- 一定要打开整个 `Lab2` 文件夹，否则可能找不到 `essay_a.txt` 和 `essay_b.txt`。
- 无锁竞态结果可能每次不同；加锁版本结果应正确。
- 编译生成的可执行文件只在本机产生，不提交到仓库。
