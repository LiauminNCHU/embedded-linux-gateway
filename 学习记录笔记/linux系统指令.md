## Touch
touch 是 Linux 里一个非常简单但实用的命令。它的主要作用有两个：

1. 创建一个空的文件（如果文件不存在）
2. 更新一个已存在文件的“时间戳”（修改时间和访问时间）

一、最常见用法：创建空文件

touch filename.txt


如果 filename.txt 不存在，它就创建一个大小为 0 的空文件。  
如果文件已经存在，它什么都不做（只更新时间戳）。

举个栗子：  
你想在 /home/project 目录下新建一个 README.md 文件，但又不想打开编辑器，直接：
touch /home/project/README.md


就会生成一个空白的 README.md。

二、第二个作用：更新时间戳

Linux 系统里每个文件都有三个时间属性：

• atime（access time）：最后一次读取文件的时间

• mtime（modify time）：最后一次修改文件内容的时间

• ctime（change time）：最后一次修改文件属性（权限、所有权等）的时间

touch 默认把文件的 atime 和 mtime 改成当前系统时间。
touch existing_file.txt


执行后，existing_file.txt 的修改时间和访问时间都会变成现在这一刻。

有什么用？

• 有些编译工具（如 make）会根据文件的 mtime 来判断是否需要重新编译。如果你改了某个头文件，但不想重新编译所有依赖，可以用 touch 假装它被修改过，骗过 make。

• 备份脚本、日志轮转等场景也需要手动调整时间戳。

三、常用选项

选项 作用

-a 只改 atime（访问时间）

-m 只改 mtime（修改时间）

-c 不创建新文件（如果文件不存在，也不报错）

-t 指定时间戳，而不是用当前时间（格式：[[CC]YY]MMDDhhmm[.ss]）

-r 参照另一个文件的时间戳（复制时间）

例子：
### 把 file1.txt 的时间戳改成和 file2.txt 一样
touch -r file2.txt file1.txt

### 把文件的时间设为 2024年1月1日 12:00
touch -t 202401011200 file.txt

### 只改访问时间，不改修改时间
touch -a file.txt


四、和嵌入式开发的关系

你在嵌入式 Linux 上编译代码、制作根文件系统时，经常会用到 touch：

• 创建临时空文件作为占位符（比如创建设备节点前的测试文件）

• 在 Buildroot 或 Yocto 构建过程中，手动调整某些文件的时间戳，触发重新编译

• 在脚本里用 touch /tmp/flag 来标记某个事件已经发生（比如开机初始化完成）

五、一句话总结

*touch 就是 Linux 里的“凭空捏造文件”和“伪造时间”的小工具。*  

文件不存在 → 建个空的；文件存在 → 刷新它的时间戳。


## Cmake & make
这是 C/C++ 项目中非常经典的 CMake 外部构建（Out-of-source build）流程。以下是这两条指令的作用以及目录中生成文件的解释：

一、cmake .. 和 make -j$(nproc) 的作用

1. cmake ..（配置与生成阶段）
• 作用：告诉 CMake 工具去上一级目录（..）找 CMakeLists.txt 文件，并根据里面的规则，生成当前平台适用的构建脚本。

• 为什么用 ..：因为你前面执行了 mkdir build 和 cd build，进入了新建的构建目录。在 build 目录里执行 cmake ..，就是现代 CMake 推荐的外部构建标准做法。它能把编译产生的中间文件全部隔离在 build 文件夹里，保持源码目录干净。

• 输出：它不直接编译代码，而是生成后续的编译指令清单（如 Makefile）。

2. make -j$(nproc)（实际编译阶段）
• 作用：make 是一个自动化构建工具，它会读取上一步生成的 Makefile，调用编译器（如 gcc/g++）把源代码实际编译并链接成可执行文件或库文件。

• -j$(nproc) 的含义：

  • -j：告诉 make 开启并行编译模式。

  • $(nproc)：这是一个 Shell 命令，会自动获取你电脑的 CPU 逻辑核心数。

  • 整体意思：让 make 同时运行与 CPU 核心数相同数量的编译任务。这能充分利用多核 CPU 的性能，把大型项目的编译时间缩短几倍甚至几十倍。

二、为什么目录生成了很多东西？

这些都是 CMake 和 make 正常工作时产生的中间工作痕迹。因为采用了外部构建，它们全被整齐地放在了 build/ 目录下，没有污染你的源码：

• Makefile：核心构建脚本。CMake 生成的指令清单，make 命令就是靠读它来完成编译、链接等操作的。

• CMakeCache.txt：CMake 的“记忆库”（缓存文件）。它保存了第一次配置时的检测结果（比如编译器路径、依赖库路径、自定义变量等）。下次再运行 cmake .. 时，它会直接读取缓存加速配置。如果修改了配置或报错，删除这个文件可以强制重新配置。

• CMakeFiles/：CMake 的内部工作区（目录）。里面存放了编译所需的中间文件、依赖关系规则、编译器检测配置以及各个目标（比如 gateway）编译后的 .o 目标文件等。一般不需要手动修改。

• cmake_install.cmake：自动生成的安装脚本。当你后续执行 make install 时，它会把编译好的程序、库或头文件拷贝到系统指定的安装目录（如 /usr/local/bin）。

• gateway：最终成果！这是通过实际编译链接后生成的可执行程序。所以最后你可以直接在终端输入 ./gateway 来运行它。

总结：你只需要知道 build/ 里面是编译过程的“厨房垃圾”和“成品”。如果哪天想清理重新编译，直接 rm -rf build 删掉整个文件夹，再重新执行那几步命令即可，你的源码目录依然干干净净。

## pipe
Linux 的 `pipe()` 是一种 IPC（进程间通信）机制。

最经典的模型：

```text
         pipe
      ┌────────┐
      │        │
写端 ──→        ──→ 读端
      │        │
      └────────┘
```

一个线程/进程：

```text
write()
```

另一个线程/进程：

```text
read()
```

---

### 2. pipe 的两个文件描述符

创建：

```c
int pipefd[2];

pipe(pipefd);
```

成功后：

```text
pipefd[0] → 读端
pipefd[1] → 写端
```

也就是：

```text
pipefd[1]
   │
   │ write()
   ↓
┌─────────┐
│  pipe   │
└─────────┘
   │
   │ read()
   ↓
pipefd[0]
```

这个地方一定记住：

> **0 是读，1 是写。**

以后看到：

```c
pipefd[0]
pipefd[1]
```

第一反应应该是：

```text
[0] read
[1] write
```

---

### 3. 为什么线程也能用 pipe？

你可能会问：

> pipe 不是“进程间通信”吗？为什么我们现在拿它做线程通信？

因为：

```text
同一个进程中的线程
        ↓
共享进程的文件描述符表
        ↓
可以访问同一个 pipe
```

所以：

```text
进程
│
├── sensor thread
│
└── monitor thread
```

两个线程可以共同使用：

```text
pipefd
```

## SIGINT
好，开始 **Signal（信号）**。这一步我们还是按照之前的方式：**先解决一个问题，不一次把所有代码塞给你。**

我们现在要解决的是：

> 你的两个线程都是 `while (1)`，按 `Ctrl+C` 后，整个进程直接被杀掉，无法执行 `pthread_join()` 和 `queue_destroy()`。

目标变成：

```text
Ctrl+C
   ↓
SIGINT
   ↓
告诉程序：我要退出了
   ↓
两个线程结束
   ↓
pthread_join()
   ↓
queue_destroy()
   ↓
正常退出
```

---

### 第一部分：先认识 SIGINT

你在终端运行：

```bash
./gateway
```

然后按：

```text
Ctrl+C
```

Linux 实际上不是简单地“杀程序”。

终端会向前台进程发送：

```text
SIGINT
```

也就是：

> **Signal Interrupt，中断信号。**

默认情况下，进程收到 `SIGINT` 后会终止。

你可以先验证：

```bash
kill -l
```
```bash
kill -l
 1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP
 6) SIGABRT      7) SIGBUS       8) SIGFPE       9) SIGKILL     10) SIGUSR1
11) SIGSEGV     12) SIGUSR2     13) SIGPIPE     14) SIGALRM     15) SIGTERM
16) SIGSTKFLT   17) SIGCHLD     18) SIGCONT     19) SIGSTOP     20) SIGTSTP
21) SIGTTIN     22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ
26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO       30) SIGPWR
31) SIGSYS      34) SIGRTMIN    35) SIGRTMIN+1  36) SIGRTMIN+2  37) SIGRTMIN+3
38) SIGRTMIN+4  39) SIGRTMIN+5  40) SIGRTMIN+6  41) SIGRTMIN+7  42) SIGRTMIN+8
43) SIGRTMIN+9  44) SIGRTMIN+10 45) SIGRTMIN+11 46) SIGRTMIN+12 47) SIGRTMIN+13
48) SIGRTMIN+14 49) SIGRTMIN+15 50) SIGRTMAX-14 51) SIGRTMAX-13 52) SIGRTMAX-12
53) SIGRTMAX-11 54) SIGRTMAX-10 55) SIGRTMAX-9  56) SIGRTMAX-8  57) SIGRTMAX-7
58) SIGRTMAX-6  59) SIGRTMAX-5  60) SIGRTMAX-4  61) SIGRTMAX-3  62) SIGRTMAX-2
63) SIGRTMAX-1  64) SIGRTMAX
```


里面应该能看到：

```text
2) SIGINT
```

---

### 第二部分：我们自己接管 SIGINT

C/Linux 提供了：

```c
signal()
```

最简单的形式：

```c
signal(SIGINT, handler);
```

意思：

> 收到 SIGINT 后，不使用默认的“直接终止”，而是调用我的 `handler()`。

例如：

```c
#include <signal.h>

void sigint_handler(int sig)
{
    printf("SIGINT received\n");
}
```

然后：

```c
signal(SIGINT, sigint_handler);
```

---

### 第三个问题：handler 里面应该做什么？

这里先不要急着写。

我们的目标是：

```text
SIGINT
  ↓
handler
  ↓
告诉两个线程：
“该退出了”
```

最简单的想法是搞一个全局变量：

```c
int running = 1;
```

线程：

```c
while (running) {
    ...
}
```

收到 Ctrl+C：

```c
running = 0;
```

于是：

```text
running = 1
   ↓
线程继续工作

Ctrl+C
   ↓
SIGINT
   ↓
running = 0
   ↓
while(running) 不成立
   ↓
线程退出
```

这个思路是对的。

但是这里马上会遇到一个非常重要的问题：

> **普通的 `int` 能不能安全地在 signal handler 和线程之间共享？**

这里就是我们要学习的：

```c
volatile sig_atomic_t
```

---

#### 现在先学这个

定义：

```c
volatile sig_atomic_t running = 1;
```

需要：

```c
#include <signal.h>
```

然后：

```c
void sigint_handler(int sig)
{
    running = 0;
}
```

线程：

```c
while (running) {
    ...
}
```

---

## 为什么不是普通 `int`？

你现在先记住两个关键词：

### `sig_atomic_t`

表示：

> 这种类型的读写对于 signal handler 来说具有原子性保证。

所以：

```c
volatile sig_atomic_t running;
```

特别适合这种：

```text
主程序/线程
      ↕
 signal handler
```

之间传递一个简单状态。

### `volatile`

这里先简单理解：

> 告诉编译器，这个变量可能在正常代码流程之外被修改，不要擅自假设它一直不变。

因为：

```c
while (running) {
    ...
}
```

期间可能突然发生：

```text
SIGINT
 ↓
handler
 ↓
running = 0
```
---
```
线程不仅要考虑“正常情况下什么时候继续”，还要考虑“程序关闭时如何从阻塞状态中出来”。
```


## 查找 端口 进程
ss -tlnp | grep :8888


## TCP 粘包现象

## TCP 多线程服务器如何实现优雅退出
```
我使用一个 volatile sig_atomic_t running 作为全局运行标志，SIGINT handler 中只修改这个标志。主线程阻塞在 accept() 时，SIGINT 会使 accept() 因 EINTR 返回，从而退出 accept 循环。

Worker 线程可能阻塞在消息队列的条件变量上，因此主线程退出时通过 pthread_cond_broadcast() 唤醒 Worker。

对于 TCP 工作线程可能阻塞在 recv() 的情况，我维护了一个客户端 socket 链表。收到退出信号后，主线程遍历所有客户端调用 shutdown(fd, SHUT_RDWR)，使阻塞的 recv() 返回，TCP 线程随后自行清理 socket 并从客户端链表中删除自己。

另外通过 client_thread_count + mutex + condition variable 等待所有 TCP 线程退出，最后再 pthread_join() Worker、销毁消息队列和关闭监听 socket。
```