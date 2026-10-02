## read
```c
read(pipefd[0], &temperature, sizeof(temperature));
```
可以理解成：

从 pipe 的读端取出数据，放进 temperature。

三个参数：

read(
    文件描述符,
    数据存放地址,
    读取多少字节
)

所以：

read(
    pipefd[0],
    &temperature,
    sizeof(temperature)
);

就是：

从 pipe 读取一个 int，放到 temperature。

## write

```c
write(
    pipefd[1],
    &temperature,
    sizeof(temperature)
);
```

意思：

> 把 temperature 里面的一个 int 写入 pipe。

于是：

```text
write
  ↓
pipe
  ↓
read
```

形成了一个数据通道。
### 这里出现一个非常重要的东西：阻塞

假设 sensor 暂时没写数据。

monitor 执行：

```c
read(pipefd[0], &temperature, sizeof(temperature));
```

怎么办？

它会等待。

也就是说：

```text
monitor
   │
   ↓
read()
   │
   │ pipe没有数据
   ↓
阻塞
   │
   │
   │        sensor
   │           │
   │           ↓
   │         write()
   │           │
   │           ↓
   └──────── pipe
               │
               ↓
           monitor继续
```

所以 pipe 本身就提供了一种非常有用的机制：

> **消费者没有数据时等待，而不是疯狂轮询。**

## mutex + condition variable

**阻塞等待。**

所以这里会使用：

```c
pthread_cond_wait()
```

代码：

```c
while (queue->count == QUEUE_SIZE) {
    pthread_cond_wait(&queue->not_full,
                      &queue->mutex);
}
```

---

### . 重要的一行

```c
pthread_cond_wait(&queue->not_full,
                  &queue->mutex);
```

你先不要死记参数。

它可以理解成：

> **“Queue 满了，我暂时睡觉；等别人告诉我 Queue 有空位了，我再继续。”**

但是它有一个非常重要的行为：

### `pthread_cond_wait()` 会自动释放 mutex

也就是说：

```text
Producer：

lock mutex
   ↓
发现 Queue 满
   ↓
cond_wait()
   ↓
释放 mutex
   ↓
睡眠
```

为什么必须释放？

因为 Consumer 需要拿 mutex：

```text
Consumer
   ↓
lock mutex
   ↓
取走一个消息
   ↓
count--
   ↓
Queue 有空位
   ↓
唤醒 Producer
```

如果 Producer 睡觉的时候一直占着 mutex：

```text
Producer
 ↓
占着 mutex
 ↓
睡觉
 ↓
Consumer 永远拿不到 mutex
```

那就死锁了。

所以：

> **`pthread_cond_wait()` = 释放 mutex + 睡眠等待 + 被唤醒后重新获得 mutex。**

这个一定要理解。

## pthread_cond_wait() 和 accept() 导致的阻塞 有什么不同
条件变量

```pthread_cond_wait(...)```

是在等待：

某个线程通知我，条件可能满足了。


```accept(...)```

是在等待：

某个客户端来建立 TCP 连接




## 现在你应该真正记住的不是代码，而是这三个东西

### ① `running`

负责回答：

> **程序还要不要继续运行？**

```c
running = 1;  // 正常运行
running = 0;  // 准备退出
```

---

### ② `pthread_cond_wait`

负责：

> **条件不满足时，让线程睡眠。**

例如：

```c
while (queue->count == 0 && *running)
    pthread_cond_wait(...);
```

---

### ③ `pthread_cond_broadcast`

负责：

> **条件发生变化时，把正在等待这个条件的线程叫醒。**

```c
pthread_cond_broadcast(&queue.not_empty);
pthread_cond_broadcast(&queue.not_full);
```

所以三者关系可以记成：

```text
             running
          程序是否继续？
               │
               ▼
        ┌──────────────┐
        │              │
        ▼              ▼
   条件满足？       程序退出？
        │              │
        ▼              ▼
    正常工作       broadcast
                       │
                       ▼
                  唤醒等待线程
                       │
                       ▼
                  检查 running
                       │
                       ▼
                    退出
```

这就是 Linux 多线程程序里非常典型的**优雅退出（graceful shutdown）**模式。

---

### 你现在这部分已经闭环了

目前已经掌握：

```text
pthread_create
       ↓
多线程
       ↓
mutex
       ↓
condition variable
       ↓
producer / consumer
       ↓
ring buffer
       ↓
signal
       ↓
running
       ↓
broadcast
       ↓
graceful shutdown
```


## TCP
```c
    char buffer[128];

    //          客户端socket  收到的数据     只接受127字节
    //                                最后的\0自己补充
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if(n < 0){
        perror("recv");

        close(client_fd);
        close(server_fd);
        return 1;
    }

    buffer[n] = '\0';
    printf("recv:%s\n", buffer);

    const char *reply = "hello from server";

    int ret = send( client_fd,
                    reply,
                    strlen(reply),
                    0);
    
    if(ret < 0){
        perror("send");
    }

    printf("client connected!\n");

    close(client_fd);
    close(server_fd);

    return 0;
}
```

### sokect()
```c
#include <arpa/inet.h>
#include <sys/socket.h>
    //创建Socket 
    int server_fd;
    //创建Socket        IPv4    TCP字节流，SOCK_DGRAM是UDP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);


        //本机测试
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
```

### bind()
```c
    //bind 绑定IP和端口
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

```

### listen()
```c
    //开始监听
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("TCP server listening on port 8888...\n");
```
### accept()
```c
//接受客户端
    int client_fd;
    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }
```


recv()
↓
网络 → 程序

send()
↓
程序 → 网络

### recv() 负责“拿字节”，消息解析负责“找消息”。