```
TCP Server
   │
   ├── ① socket()
   ├── ② bind()
   ├── ③ listen()
   ├── ④ accept()
   │
   ▼
建立 TCP 连接
   │
   ├── ⑤ recv()
   ├── ⑥ send()
   │
   ▼
客户端 ↔ 服务端双向通信
   │
   ▼
字符串/结构化数据
   │
   ▼
TCP + 线程
   │
   ▼
TCP接收线程
   │
   ▼
message_queue

Linux 应用
│
├── C 语言基础                  ✅
├── pthread 多线程              ✅
│   ├── pthread_create          ✅
│   ├── pthread_join            ✅
│   ├── mutex                   ✅
│   ├── condition variable      ✅
│   └── graceful shutdown       ✅
│
├── message_queue               ✅
│   └── 环形缓冲区              ✅
│
├── TCP
│   ├── socket                  ✅
│   ├── bind                    ✅
│   ├── listen                  ✅
│   ├── accept                  ✅
│   ├── recv                    ✅
│   ├── send                    ✅
│   └── TCP + thread             ✅ 当前
│
├── TCP 多客户端                ⬅️ 下一步
├── TCP 消息协议/粘包拆包       ⬅️
├── TCP → message_queue         ⬅️
│
├── UDP
├── MQTT
├── SQLite
├── UART
├── GPIO / IO
├── 日志
├── 配置文件
└── 综合起来
```