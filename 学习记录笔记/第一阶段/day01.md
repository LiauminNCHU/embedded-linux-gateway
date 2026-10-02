# Linux 多线程、条件变量与生产者-消费者模型

## 一、总体结构

今天学习的核心目标：

> 从 Linux 多线程开始，逐步实现一个线程安全的生产者-消费者模型，并解决程序退出时线程无法正常结束的问题。

整体知识链：

```text
pthread_create
    ↓
多线程
    ↓
共享数据
    ↓
Mutex 互斥锁
    ↓
Condition Variable 条件变量
    ↓
Producer / Consumer
    ↓
Ring Buffer 环形队列
    ↓
Signal 信号
    ↓
running 运行状态
    ↓
broadcast 唤醒等待线程
    ↓
Graceful Shutdown 优雅退出
```

当前项目中的线程模型：

```text
                 ┌──────────────┐
                 │ sensor线程   │
                 │ Producer     │
                 └──────┬───────┘
                        │
                        │ queue_push()
                        ▼
                ┌────────────────┐
                │  Message Queue │
                │  Ring Buffer   │
                └───────┬────────┘
                        │
                        │ queue_pop()
                        ▼
                 ┌──────────────┐
                 │ monitor线程  │
                 │ Consumer     │
                 └──────────────┘
```

程序退出流程：

```text
Ctrl+C
   ↓
SIGINT
   ↓
sigint_handler()
   ↓
running = 0
   ↓
main检测到running=0
   ↓
pthread_cond_broadcast()
   ↓
唤醒阻塞线程
   ↓
push/pop检查running
   ↓
线程退出
   ↓
pthread_join()
   ↓
queue_destroy()
```

---

# 二、pthread 多线程

## 2.1 什么是线程？

线程是进程内部的执行流。

一个进程可以拥有多个线程：

```text
Process
│
├── Thread A
├── Thread B
└── Thread C
```

同一个进程中的线程共享：

* 全局变量
* 堆
* 文件描述符
* 地址空间

但是每个线程拥有自己的：

* 栈
* 寄存器状态
* 程序计数器

因此多个线程可以同时执行不同任务。

---

## 2.2 pthread_create()

创建线程：

```c
pthread_t worker;

pthread_create(
    &worker,
    NULL,
    worker_thread,
    NULL
);
```

函数原型：

```c
int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg
);
```

四个参数：

### 第一个参数

```c
&worker
```

用于保存新线程的线程 ID。

注意：

> `pthread_t worker` 是线程 ID 变量，不是“线程本身”。

---

### 第二个参数

```c
NULL
```

表示使用默认线程属性。

---

### 第三个参数

```c
worker_thread
```

线程启动以后执行的函数。

函数形式：

```c
void *worker_thread(void *arg)
```

---

### 第四个参数

```c
NULL
```

传递给线程函数的参数。

也就是说：

```c
pthread_create(&worker, NULL, worker_thread, NULL);
```

相当于告诉系统：

> 创建一个新线程，让它去执行 `worker_thread(NULL)`。

但它和直接调用：

```c
worker_thread(NULL);
```

有本质区别。

直接调用是在当前线程执行。

`pthread_create()` 是创建新的执行流。

---

# 三、pthread_join()

```c
pthread_join(worker, NULL);
```

作用：

> 等待指定线程结束。

例如：

```text
main
 │
 ├── 创建 sensor
 ├── 创建 monitor
 │
 ├── join(sensor)
 │       ↓
 │    等待 sensor
 │
 └── join(monitor)
         ↓
      等待 monitor
```

如果线程内部：

```c
while (1) {
    ...
}
```

那么线程永远不会结束。

此时：

```c
pthread_join(...)
```

也会一直等待。

所以：

> `pthread_join()` 本身不会让线程退出，它只是等待线程退出。

---

# 四、共享数据与线程安全

多个线程可以同时访问同一个全局变量。

例如：

```c
int counter = 0;
```

两个线程同时执行：

```c
counter++;
```

看起来只有一条语句，但实际上可以理解为：

```text
读取 counter
    ↓
加 1
    ↓
写回 counter
```

如果两个线程同时执行，就可能发生竞争。

因此：

> 多线程访问共享数据时，需要考虑线程安全。

---

# 五、Mutex 互斥锁

## 5.1 Mutex 是什么？

Mutex = Mutual Exclusion，互斥锁。

核心作用：

> 保证同一时刻只有一个线程进入临界区。

例如：

```c
pthread_mutex_lock(&mutex);

/* 临界区 */

pthread_mutex_unlock(&mutex);
```

可以理解成：

```text
线程A
  ↓
lock
  ↓
进入临界区
  ↓
访问共享数据
  ↓
unlock
  ↓
其他线程才能进入
```

---

## 5.2 临界区

临界区：

> 多线程环境中访问共享资源、必须保证互斥执行的代码区域。

例如：

```c
pthread_mutex_lock(&queue->mutex);

queue->buffer[queue->tail] = *message;
queue->tail = (queue->tail + 1) % QUEUE_SIZE;
queue->count++;

pthread_mutex_unlock(&queue->mutex);
```

这里：

```text
buffer
tail
count
```

属于队列的共享状态。

因此修改它们时需要加锁。

---

## 5.3 为什么一定要 unlock？

如果：

```c
pthread_mutex_lock(&mutex);

/* 操作 */

```

忘记：

```c
pthread_mutex_unlock(&mutex);
```

那么其他线程可能一直无法获得这个锁。

结果可能导致：

```text
线程A
 ↓
持有mutex
 ↓
一直不释放

线程B
 ↓
pthread_mutex_lock()
 ↓
一直等待
```

这类问题可能导致死锁或线程永久阻塞。

---

# 六、Condition Variable 条件变量

## 6.1 条件变量解决什么问题？

Mutex 解决：

> **谁可以访问共享数据？**

Condition Variable 解决：

> **什么时候可以继续执行？**

例如消费者：

```c
while (queue->count == 0) {
    pthread_cond_wait(
        &queue->not_empty,
        &queue->mutex
    );
}
```

意思是：

> 如果队列为空，消费者暂时不能继续消费，因此进入等待。

---

# 七、pthread_cond_wait()

核心函数：

```c
pthread_cond_wait(&cond, &mutex);
```

它非常重要。

执行过程可以理解成：

```text
线程持有 mutex
       ↓
发现条件不满足
       ↓
pthread_cond_wait()
       ↓
释放 mutex
       ↓
线程进入等待
       ↓
其他线程修改共享数据
       ↓
pthread_cond_signal/broadcast
       ↓
线程被唤醒
       ↓
重新获得 mutex
       ↓
继续执行
```

所以：

> `pthread_cond_wait()` 不会结束线程，也不会重新执行函数。

它只是让当前线程暂时睡眠。

---

# 八、为什么 pthread_cond_wait() 要放在 while 中？

推荐：

```c
while (queue->count == 0) {
    pthread_cond_wait(...);
}
```

而不是：

```c
if (queue->count == 0) {
    pthread_cond_wait(...);
}
```

原因：

> 线程被唤醒后，条件不一定真的满足，因此必须重新检查条件。

正确思维：

```text
被唤醒
   ↓
重新检查条件
   ↓
条件满足 → 继续
条件不满足 → 再次等待
```

所以条件变量的经典写法是：

```c
while (条件不满足) {
    pthread_cond_wait(...);
}
```

---

# 九、生产者-消费者模型

## 9.1 什么是生产者-消费者？

Producer：

> 产生数据。

Consumer：

> 消费数据。

中间使用队列进行缓冲：

```text
Producer
   │
   ▼
┌──────────────┐
│    Queue     │
└──────────────┘
   │
   ▼
Consumer
```

本项目：

```text
sensor_thread
     │
     │ Producer
     ▼
queue_push()
     │
     ▼
Message Queue
     │
     ▼
queue_pop()
     │
     ▼
monitor_thread
     │
     │ Consumer
```

---

# 十、Ring Buffer 环形队列

本项目没有使用无限增长的链表，而是使用固定大小数组：

```c
#define QUEUE_SIZE 10

sensor_message_t buffer[QUEUE_SIZE];
```

队列有三个核心变量：

```c
int head;
int tail;
int count;
```

含义：

### head

> 下一次读取的位置。

### tail

> 下一次写入的位置。

### count

> 当前队列中有多少个元素。

---

## 10.1 为什么使用取模？

```c
queue->tail =
    (queue->tail + 1) % QUEUE_SIZE;
```

例如：

```text
QUEUE_SIZE = 10

tail = 0
1
2
...
8
9
0
1
...
```

到达数组末尾以后重新回到开头。

因此称为：

> Ring Buffer / Circular Buffer

---

# 十一、queue_push()

生产者向队列写入：

```c
queue->buffer[queue->tail] = *message;

queue->tail =
    (queue->tail + 1) % QUEUE_SIZE;

queue->count++;
```

这里：

```c
queue->buffer[queue->tail] = *message;
```

是结构体赋值。

不是指针赋值。

也就是说：

> 把 `message` 指向的结构体内容复制到队列中。

---

# 十二、Queue 满时怎么办？

如果：

```c
queue->count == QUEUE_SIZE
```

说明队列已经满了。

当前设计：

> Producer 阻塞等待。

```c
while (queue->count == QUEUE_SIZE && *running) {
    pthread_cond_wait(
        &queue->not_full,
        &queue->mutex
    );
}
```

消费者取走一个数据：

```text
count = 10
   ↓
consumer pop
   ↓
count = 9
   ↓
pthread_cond_signal(not_full)
   ↓
producer 被唤醒
```

---

# 十三、Queue 空时怎么办？

消费者：

```c
while (queue->count == 0 && *running) {
    pthread_cond_wait(
        &queue->not_empty,
        &queue->mutex
    );
}
```

生产者放入数据：

```text
count = 0
   ↓
producer push
   ↓
count = 1
   ↓
pthread_cond_signal(not_empty)
   ↓
consumer 被唤醒
```

因此：

```text
not_empty
    ↓
通知消费者：队列可能有数据了

not_full
    ↓
通知生产者：队列可能有空间了
```

---

# 十四、pthread_cond_signal()

```c
pthread_cond_signal(&queue->not_empty);
```

作用：

> 唤醒等待这个条件变量的一个线程。

例如：

```text
Producer push
    ↓
Queue 从空变成非空
    ↓
signal(not_empty)
    ↓
唤醒一个 Consumer
```

---

# 十五、pthread_cond_broadcast()

```c
pthread_cond_broadcast(&queue->not_empty);
```

作用：

> 唤醒所有正在等待这个条件变量的线程。

区别：

```text
signal
  ↓
唤醒一个

broadcast
  ↓
唤醒所有
```

本项目在正常数据生产/消费过程中主要使用：

```c
pthread_cond_signal()
```

程序关闭时使用：

```c
pthread_cond_broadcast()
```

因为关闭时需要确保所有可能阻塞的线程都被唤醒。

---

# 十六、Signal 信号

## 16.1 SIGINT

终端按：

```text
Ctrl+C
```

通常会产生：

```text
SIGINT
```

程序可以注册处理函数：

```c
signal(SIGINT, sigint_handler);
```

---

## 16.2 信号处理函数

```c
volatile sig_atomic_t running = 1;

void sigint_handler(int sig)
{
    running = 0;
}
```

作用：

```text
Ctrl+C
   ↓
SIGINT
   ↓
sigint_handler()
   ↓
running = 0
```

---

# 十七、为什么使用 volatile sig_atomic_t？

```c
volatile sig_atomic_t running;
```

这里有两个关键点。

### sig_atomic_t

用于信号处理场景下进行简单、原子的状态访问。

这里我们只做：

```c
running = 0;
```

因此适合。

### volatile

告诉编译器：

> 这个变量可能在正常程序流程之外发生变化，不要假设它的值一直不变。

所以：

```c
volatile sig_atomic_t running = 1;
```

适合作为简单的程序运行状态标志。

---

# 十八、为什么不能在 signal handler 中直接 broadcast？

不要这样：

```c
void sigint_handler(int sig)
{
    running = 0;

    pthread_cond_broadcast(...);   // 不推荐
}
```

原因：

> pthread 的条件变量操作不是适合在异步信号处理函数中直接调用的操作。

因此本项目采用：

```text
Signal Handler
     │
     ▼
running = 0
```

然后由正常程序流程中的 main：

```text
main
 ↓
发现 running == 0
 ↓
pthread_cond_broadcast()
```

这样职责更加清晰。

---

# 十九、为什么只设置 running=0 还不够？

这是今天非常重要的实验结论。

假设：

```text
Queue 为空
   ↓
Consumer
   ↓
pthread_cond_wait()
```

此时：

```text
Ctrl+C
 ↓
running = 0
```

但是：

```text
pthread_cond_wait()
```

并不知道 `running` 发生了变化。

线程仍然可能睡眠。

所以必须：

```c
pthread_cond_broadcast(&queue.not_empty);
```

把它唤醒。

---

# 二十、可取消的条件等待

因此最终写成：

```c
while (queue->count == 0 && *running) {
    pthread_cond_wait(
        &queue->not_empty,
        &queue->mutex
    );
}

if (!*running) {
    pthread_mutex_unlock(&queue->mutex);
    return -1;
}
```

逻辑：

```text
队列为空？
   │
   ├── 否 → 正常pop
   │
   └── 是
        │
        ▼
    running == 1？
        │
        ├── 是 → wait
        │
        └── 否 → return -1
```

这就是：

> **带退出条件的阻塞等待。**

---

# 二十一、queue_push() 的退出处理

生产者同样需要考虑关闭：

```c
while (queue->count == QUEUE_SIZE && *running) {
    pthread_cond_wait(
        &queue->not_full,
        &queue->mutex
    );
}

if (!*running) {
    pthread_mutex_unlock(&queue->mutex);
    return -1;
}
```

否则可能出现：

```text
Queue 满
 ↓
Producer 永久等待
 ↓
Ctrl+C
 ↓
running = 0
 ↓
Producer 仍然阻塞
```

因此：

> 所有可能长期阻塞的线程，都需要设计退出路径。

---

# 二十二、函数返回值为什么要检查？

线程中：

```c
if (queue_push(&queue, &message, &running) != 0)
    break;
```

以及：

```c
if (queue_pop(&queue, &message, &running) != 0)
    break;
```

因为：

```text
返回 0
 ↓
正常完成

返回 -1
 ↓
程序正在关闭 / 操作没有正常完成
 ↓
退出线程
```

不能忽略返回值后继续处理无效数据。

---

# 二十三、优雅退出 Graceful Shutdown

最终程序退出流程：

```text
                    Ctrl+C
                       │
                       ▼
                     SIGINT
                       │
                       ▼
               running = 0
                       │
                       ▼
              main发现running=0
                       │
             ┌─────────┴─────────┐
             ▼                   ▼
     broadcast(not_empty)  broadcast(not_full)
             │                   │
             ▼                   ▼
       唤醒consumer          唤醒producer
             │                   │
             └─────────┬─────────┘
                       ▼
                检查 running
                       │
                       ▼
                  return -1
                       │
                       ▼
                 线程退出
                       │
                       ▼
                 pthread_join()
                       │
                       ▼
                queue_destroy()
                       │
                       ▼
                   程序退出
```

这叫：

> **Graceful Shutdown（优雅退出）**

核心不是简单的：

```c
exit(0);
```

而是：

```text
通知停止
 ↓
唤醒阻塞线程
 ↓
线程自己退出
 ↓
等待线程结束
 ↓
释放资源
 ↓
程序退出
```

---

# 二十四、资源初始化与销毁

Queue 初始化：

```c
queue_init(&queue);
```

初始化：

```text
head
tail
count
mutex
not_empty
not_full
```

销毁：

```c
queue_destroy(&queue);
```

销毁：

```c
pthread_cond_destroy(&queue->not_full);
pthread_cond_destroy(&queue->not_empty);
pthread_mutex_destroy(&queue->mutex);
```

---

# 二十五、初始化失败时的资源回滚

例如：

```c
pthread_mutex_init()
```

成功。

但是：

```c
pthread_cond_init(&queue->not_empty)
```

失败。

那么已经创建的 mutex 就必须销毁：

```c
pthread_mutex_destroy(&queue->mutex);
```

如果第三步失败：

```text
mutex        √
not_empty    √
not_full     ×
```

需要回滚：

```text
destroy not_empty
destroy mutex
```

所以：

> 初始化过程中的部分失败，也需要释放已经成功创建的资源。

---

# 二十六、今天最容易犯的错误

## 错误1：忘记 unlock

```c
pthread_mutex_lock(&mutex);

/* ... */

return -1;
```

错误。

应该：

```c
pthread_mutex_unlock(&mutex);
return -1;
```

---

## 错误2：条件变量使用 if

不推荐：

```c
if (queue->count == 0)
    pthread_cond_wait(...);
```

应该：

```c
while (queue->count == 0)
    pthread_cond_wait(...);
```

---

## 错误3：Ctrl+C 后只设置 running=0

```c
running = 0;
```

并不能自动唤醒：

```c
pthread_cond_wait()
```

还需要：

```c
pthread_cond_broadcast()
```

---

## 错误4：在 signal handler 中调用 pthread_cond_broadcast()

不应该这样做。

应该：

```text
signal handler
    ↓
只修改 running
    ↓
main正常流程
    ↓
broadcast
```

---

## 错误5：忽略 push/pop 返回值

不应该：

```c
queue_pop(...);
printf("receive ...");
```

应该：

```c
if (queue_pop(...) != 0)
    break;

printf("receive ...");
```

---

## 错误6：把结构体当指针清空

错误思路：

```c
queue->buffer[index] = NULL;
```

因为：

```c
buffer[index]
```

是：

```c
sensor_message_t
```

不是指针。

当前环形队列只需要维护：

```text
head
tail
count
```

不需要把已经取出的结构体手动设置成 NULL。

---

# 二十七、面试时如何描述这个项目

可以这样回答：

> 我实现了一个基于 pthread 的生产者消费者模型。传感器线程作为生产者，将传感器数据写入固定大小的环形队列，监控线程作为消费者从队列读取数据。
>
> 为保证线程安全，我使用 mutex 保护队列的共享状态，包括 head、tail 和 count；使用两个 condition variable 分别处理队列非空和非满的等待与唤醒。
>
> 当队列满时，生产者等待 not_full；当队列为空时，消费者等待 not_empty。正常生产消费时使用 pthread_cond_signal 唤醒等待线程。
>
> 另外，我实现了 SIGINT 信号处理和 graceful shutdown。Ctrl+C 后通过 running 标志通知线程退出，并由 main 线程 broadcast 唤醒可能阻塞在 condition variable 上的线程，最后通过 pthread_join 等待线程结束并释放 mutex 和 condition variable 等资源。

---

# 二十八、今天真正需要掌握的核心

如果以后复习时间很短，至少记住：

```text
1. pthread_create
   → 创建线程

2. pthread_join
   → 等待线程结束

3. mutex
   → 保护共享数据

4. condition variable
   → 等待某个条件成立

5. pthread_cond_wait
   → 释放mutex并等待，被唤醒后重新获得mutex

6. pthread_cond_signal
   → 唤醒一个等待线程

7. pthread_cond_broadcast
   → 唤醒所有等待线程

8. producer / consumer
   → 生产者产生数据，消费者处理数据

9. ring buffer
   → 固定大小的循环队列

10. SIGINT
    → Ctrl+C产生的中断信号

11. running
    → 控制程序是否继续运行

12. graceful shutdown
    → 通知 → 唤醒 → 线程退出 → join → 释放资源
```

---

# 二十九、一句话理解整个系统

> **Mutex 管“谁能访问”，Condition Variable 管“什么时候能访问”，Queue 管“数据放在哪里”，running 管“程序还要不要运行”，broadcast 负责“程序关闭时把睡着的线程叫醒”。**
