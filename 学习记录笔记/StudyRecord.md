可以。按照你这个岗位，我不建议找一个“单纯 MQTT 库”作为项目，而是找一个**完整的 Linux 应用层项目**：能体现 C/C++、多线程、网络、IO、MQTT、SQLite、CMake、交叉编译，并且最好还能往 Buildroot/板卡部署延伸。

我先给你一个我认为**最适合你现在做简历项目**的方向：

## 推荐项目：做一个「嵌入式 Linux 智能网关」

核心可以基于开源 MQTT C 客户端 Eclipse Paho 来搭建，而不是单纯把 Paho 本身当成项目。

![Image](https://images.openai.com/static-rsc-4/XWqbOGXMuIvXGgJgfWoMO0fBhdwZVMQ-eXmvEMw740Mck7JqftMlBf9MC90rArqhwDdJu_mOb71iykpGFBF77a12AO-AJaEr0qWi982cEFPq8iOTWFvJnVU28P_7bk0rcKvz9lf3kD-s1Z4dRiONiqoPtgEVZgHGVT-4v5Z7WAyqbDAIX6Z6wLLc8mHr4znw?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/FnrNENO4IDccITySO_kOrcA_ypBVam8p8xokaUKsrtixKWyxmnFvoaCBXTkjMjPJcPzc8NNhWVVMtvhOPeUBkVGMsYdoNtyRFoRs6CODgbNgN8MyH3W3mrYtwyq1__2NTmFTlGgYGCyo4PzC1ktgeoC0_FrF9o6WX-Pj8ZGZPT74Gv8YAsmxvICr3cU_YoYz?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/s7G8l_1Lk8mgngCAboP5w391UddFAVzVtNiHd0ybRXCevSN-bBuyuRdf2McPJVFbMpPEVPkhjObT5LrlSCk1d-t4a3zBT4NShqeOaXUAZ3voeXXs50wxJ1zWBr5gsiNXfVxC6OLdD9GmDZfxPnJ7wvc3r8Rfp0si79_wqxlpngs6FGmTZnIakONoGugJ8PXc?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/LgykaKrqyeIhi5qaXtu-8laSTVoCJKENLbLOqPZpHBqYk3pjkV-rujw34XXeILrGK2ss2ffx9PgM2PYAMMmQsvQhwTjulgoKsqSU1OJzwUSbSKVq9RNMbKYUxK5bRJ56UUwzhZxYM-98URZksNwdVvuTBaNfoKcUSnXbUprzx5sC4QlUKbTToSBX-ffro1rN?purpose=fullsize)

![Image](https://images.openai.com/static-rsc-4/B6jprb5W03MhC1cxItOo9WYgSrXjjAV45Q1QoUTJ1qCyUBIqlceCghyBb8U5C0YvdGQ7Cia7m7UQEWCOIcGTiNy0jap5v-TzsqmWtkQe7aIP84t2yxuuDkShXzdOl0cr0Ax99E3WinbdPLS82mnBmSCI53hvK0FTUMBSYkqV7Qi4rZ2SNCSSpi99BksyMiz1?purpose=fullsize)

官方的 [Eclipse Paho MQTT C](https://github.com/eclipse-paho/paho.mqtt.c?utm_source=chatgpt.com) 本身就是 Linux/ARM 可构建的 C MQTT 客户端，并提供 CMake 构建和 ARM 交叉编译支持。([GitHub][1])

但**重点不是复现 Paho**，而是：

> **以 Paho + SQLite + socket + pthread 为基础，自己做一个 Linux IoT Gateway 应用。**

这就非常贴你的岗位。

---

# 一、项目最终长什么样

可以把项目命名为：

> **Embedded Linux IoT Gateway**
>
> 基于 C/C++、MQTT、SQLite、pthread、Socket 的嵌入式 Linux 智能网关

整体结构：

```text
                  ┌─────────────────────┐
                  │      MQTT Server     │
                  │    EMQX / Mosquitto │
                  └──────────┬──────────┘
                             │ MQTT
                             ↓
┌────────────────────────────────────────────────┐
│             Embedded Linux Gateway              │
│                                                │
│ ┌─────────────┐       ┌────────────────────┐  │
│ │ MQTT Thread │──────→│ Message Dispatcher │  │
│ └─────────────┘       └─────────┬──────────┘  │
│                                 │              │
│                    ┌────────────┼──────────┐  │
│                    ↓            ↓          ↓  │
│              ┌─────────┐ ┌──────────┐ ┌──────┐│
│              │ Device  │ │ Database │ │Alarm ││
│              │ Manager │ │ SQLite   │ │      ││
│              └────┬────┘ └──────────┘ └──────┘│
│                   │                            │
│             ┌─────┴─────┐                      │
│             ↓           ↓                      │
│          GPIO/LED     UART                     │
│                                                │
│ ┌────────────────────────────────────────────┐ │
│ │ TCP/UDP Socket Server                      │ │
│ └────────────────────────────────────────────┘ │
└────────────────────────────────────────────────┘
```

这个项目一旦做完整，岗位要求里面的大部分东西都能讲。

---

# 二、为什么我觉得它比单纯找 GitHub 项目更适合你

你这个岗位实际上分成三层：

### 第一层：Linux 应用

```text
C
├── pthread
├── socket
├── TCP/UDP
├── select/poll/epoll
├── pipe
├── signal
├── file IO
└── serial
```

### 第二层：嵌入式应用

```text
MQTT
SQLite
UART
GPIO
日志
配置文件
守护进程
```

### 第三层：嵌入式 Linux

```text
U-Boot
Kernel
Device Tree
RootFS
Cross Compile
Buildroot
```

这个项目可以从第一层一路做到第三层。

---

# 三、第一阶段：先做 Linux 应用

先不要碰驱动。

目录可以设计成：

```text
embedded-linux-gateway/
├── CMakeLists.txt
├── README.md
│
├── src/
│   ├── main.c
│   │
│   ├── mqtt/
│   │   ├── mqtt_client.c
│   │   └── mqtt_client.h
│   │
│   ├── network/
│   │   ├── tcp_server.c
│   │   ├── udp_server.c
│   │   └── network.h
│   │
│   ├── device/
│   │   ├── device_manager.c
│   │   └── device_manager.h
│   │
│   ├── database/
│   │   ├── database.c
│   │   └── database.h
│   │
│   ├── uart/
│   │   ├── uart.c
│   │   └── uart.h
│   │
│   ├── log/
│   │   ├── log.c
│   │   └── log.h
│   │
│   └── utils/
│       ├── config.c
│       └── config.h
│
├── config/
│   └── gateway.conf
│
├── database/
│   └── gateway.db
│
├── scripts/
│   ├── start.sh
│   └── stop.sh
│
└── docs/
    └── architecture.md
```

这个目录本身就很像一个真正的 Linux 应用项目。

---

# 四、你要重点实现的 6 个模块

## ① MQTT

这是这个项目的核心。

例如：

```text
gateway/device/001/status
gateway/device/001/temperature
gateway/device/001/control
```

设备产生：

```json
{
    "device_id": 1,
    "temperature": 26.5,
    "humidity": 61
}
```

通过 MQTT：

```text
Device
   ↓
MQTT
   ↓
Gateway
   ↓
SQLite
```

Paho C 官方项目支持 CMake，也提供 Linux 和 ARM 构建方式。([GitHub][1])

---

# 五、② 多线程

这是你面试非常值得讲的地方。

例如：

```text
main
 │
 ├── MQTT thread
 │
 ├── TCP server thread
 │
 ├── UART thread
 │
 ├── database thread
 │
 └── watchdog thread
```

然后使用：

```c
pthread_create()
pthread_mutex_lock()
pthread_mutex_unlock()
pthread_cond_wait()
pthread_cond_signal()
```

做一个：

```text
MQTT线程
    ↓
消息队列
    ↓
Worker线程
    ↓
SQLite
```

这样你就可以回答：

> 为什么不能直接在 MQTT 回调里面操作数据库？

> 因为 MQTT 回调属于通信路径，如果数据库操作阻塞，会影响消息接收，因此采用生产者消费者模型，把消息放入线程安全队列，由 Worker 异步处理。

这个就已经不是“背八股”了。

---

# 六、③ TCP/UDP Socket

再做一个本地管理接口：

```text
PC
 │
 │ TCP
 ↓
Gateway
```

例如：

```bash
$ nc 192.168.1.100 9000
```

输入：

```text
get temperature
```

返回：

```text
temperature: 26.5
```

或者：

```text
set led 1
```

然后：

```text
TCP Server
     ↓
Device Manager
     ↓
GPIO
     ↓
LED
```

这样岗位里的：

> socket网络编程（TCP/UDP）

就真正有项目对应。

---

# 七、④ SQLite

SQLite 用来保存设备信息：

```sql
CREATE TABLE device (
    id INTEGER PRIMARY KEY,
    name TEXT,
    type TEXT,
    status INTEGER
);
```

传感器数据：

```sql
CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY,
    device_id INTEGER,
    temperature REAL,
    humidity REAL,
    timestamp INTEGER
);
```

然后实现：

```text
MQTT
 ↓
Message Queue
 ↓
Database Worker
 ↓
SQLite
```

这样岗位里面的：

> SQLite

也覆盖了。

---

# 八、⑤ UART

这个非常适合你后面放到 RK3568 上。

例如：

```text
STM32
  │
  │ UART
  ↓
RK3568
  │
  ├── parse sensor data
  │
  ├── SQLite
  │
  └── MQTT
       ↓
      云端
```

最终就变成：

```text
STM32
  ↓ UART
RK3568
  ↓ MQTT
MQTT Broker
  ↓
PC / Web
```

这已经非常接近真实嵌入式 Linux 产品。

---

# 九、⑥ GPIO

你已经在学习 RK3568 的：

```text
/sys/class/leds/
```

这一块，所以可以直接结合进去。

例如：

```bash
echo 1 > /sys/class/leds/work/brightness
```

你的 Gateway 程序可以封装：

```c
led_set(LED_WORK, 1);
```

然后：

```text
MQTT
 ↓
"led=1"
 ↓
Gateway
 ↓
LED Manager
 ↓
/sys/class/leds/work/brightness
 ↓
RK3568 LED
```

这就把你现在学的 DTS / LED 子系统和 Linux 应用开发连起来了。

---

# 十、最后再加 Buildroot

这是这个项目非常重要的一步。

你岗位明确写了：

> 内核裁剪、Yocto/Buildroot

那么最后不要停留在：

```text
Ubuntu
 ↓
./gateway
```

而是做：

```text
Buildroot
   │
   ├── Linux Kernel
   ├── BusyBox
   ├── RootFS
   ├── SQLite
   ├── MQTT
   └── gateway
          ↓
      RK3568
```

最终启动：

```text
U-Boot
   ↓
Linux Kernel
   ↓
RootFS
   ↓
gateway.service
   ↓
IoT Gateway
```

这时候你的项目就同时覆盖：

| 岗位要求      | 项目对应      |
| --------- | --------- |
| C         | Gateway   |
| Shell     | 启停脚本      |
| grep/find | 调试        |
| 进程        | Gateway   |
| pthread   | 多线程       |
| signal    | 优雅退出      |
| pipe      | IPC       |
| TCP       | 管理接口      |
| UDP       | 数据发现      |
| MQTT      | 云端通信      |
| SQLite    | 数据存储      |
| UART      | 设备通信      |
| GPIO      | 硬件控制      |
| CMake     | 编译        |
| Git       | 项目管理      |
| 交叉编译      | RK3568    |
| U-Boot    | 启动        |
| Kernel    | Linux     |
| RootFS    | Buildroot |
| Buildroot | 系统构建      |

**这就非常贴你的岗位。**

---

# 十一、GitHub 上的开源基础项目

如果你希望**不是从 0 开始造轮子**，我建议把下面几个项目作为技术基础。

### 1. Paho MQTT C

[Eclipse Paho MQTT C](https://github.com/eclipse-paho/paho.mqtt.c?utm_source=chatgpt.com)

这是最推荐的。

它本身就是 C MQTT 客户端，支持 Linux，并且官方提供 CMake 和 ARM toolchain 示例。([GitHub][1])

---

### 2. MQTT-C

[MQTT-C](https://github.com/LiamBindle/MQTT-C?utm_source=chatgpt.com)

这个甚至更加适合你研究源码。

它是：

> **纯 C 的 MQTT 3.1.1 client**

而且代码量相对 Paho 更容易阅读，同时提供 CMake 和 Makefile。([GitHub][2])

---

### 3. Buildroot

[Buildroot](https://github.com/buildroot/buildroot?utm_source=chatgpt.com)

最后用它做：

```text
RK3568
 ↓
Bootloader
 ↓
Kernel
 ↓
RootFS
 ↓
Gateway
```

Buildroot 本身就是用来生成完整嵌入式 Linux 系统的工具链/构建系统，而且 Paho MQTT C 也已经有对应的 Buildroot package。([Fossies][3])

---

# 十二、但我不建议你直接“找一个现成项目照抄”

因为你的目标是：

> **秋招 Linux 应用开发 / 嵌入式 Linux**

而不是单纯完成一个 GitHub repo。

你真正需要的是：

```text
开源组件
     ↓
理解源码
     ↓
自己设计架构
     ↓
自己实现 Gateway
     ↓
交叉编译
     ↓
部署 RK3568
     ↓
Buildroot
     ↓
GitHub README
     ↓
面试讲项目
```

尤其是你已经有 **RK3568 E01**，这个项目非常适合最终直接落到你的板子上。

---

## 我建议我们把这个项目控制在 2～3 周

不要一上来做巨大项目。

### 第 1 阶段

```text
CMake
pthread
TCP
UDP
pipe
signal
```

先把 Linux 应用基本功补起来。

### 第 2 阶段

```text
MQTT
SQLite
线程池/消息队列
UART
GPIO
```

做出真正的 Gateway。

### 第 3 阶段

```text
ARM64 cross compile
        ↓
RK3568
        ↓
Buildroot
        ↓
Kernel + RootFS
        ↓
Gateway
```

最后 GitHub README 做成：

```text
项目背景
系统架构图
线程模型
MQTT通信流程
TCP通信流程
SQLite设计
UART设计
交叉编译
RK3568部署
Buildroot
运行截图
问题与解决方案
```

**这个项目比“我做过一个 MQTT demo”强很多。**

如果按你的岗位匹配度，我会给这个方案大约 **9/10**；尤其适合你现在已经有 RK3568、正在学 Linux/DTS/LED，又想转向 Linux 应用开发的路线。

继续把项目落到可执行方案

* 整理成简历项目描述
* 拆成两周开发计划
* 先搭最小可运行版本

[1]: https://github.com/eclipse-paho/paho.mqtt.c?utm_source=chatgpt.com "GitHub - eclipse-paho/paho.mqtt.c: An Eclipse Paho C client library for MQTT for Windows, Linux and MacOS. API documentation: https://eclipse-paho.github.io/paho.mqtt.c/ · GitHub"
[2]: https://github.com/LiamBindle/MQTT-C?utm_source=chatgpt.com "GitHub - LiamBindle/MQTT-C: A portable MQTT C client for embedded systems and PCs alike. · GitHub"
[3]: https://fossies.org/diffs/buildroot/2026.05.2_vs_2026.08/package/paho-mqtt-c/paho-mqtt-c.mk-diff.html?utm_source=chatgpt.com "buildroot: ...qtt-c/paho-mqtt-c.mk - 2026.05.2 vs. 2026.08 changes | Fossies Diffs"
