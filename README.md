# ChatRoom 01 第一阶段

## 第一阶段：单机版聊天室

一个基于 C++ 实现的单机版聊天室项目，用于练习 Linux 环境下的 C++ 基础开发能力。

这是聊天室项目的第一阶段，当前版本采用本地模拟聊天方式，不涉及网络通信。项目重点在于练习面向对象设计、STL 容器、文件持久化以及工程结构组织，为后续 TCP 聊天室开发打基础。

---

## 第一阶段目标

第一阶段的目标不是实现真正的网络聊天室，而是完成一个本地模拟聊天系统，支持基本的用户管理和消息管理功能。

通过这个项目，我主要练习：

- C++ 基础语法
- STL 容器使用
- 类设计与模块化
- 文件读写与数据持久化
- Linux 下的编译与运行

---

## 已实现功能

- 添加用户
- 删除用户
- 查看用户列表
- 发送消息
- 查看聊天记录
- 清空聊天记录
- 用户信息保存到本地文件
- 消息记录保存到本地文件

---

## 项目结构

```text
chatroom_01/
├── include/
│   ├── user.h
│   ├── message.h
│   └── chatroom.h
├── src/
│   ├── main.cpp
│   ├── user.cpp
│   ├── message.cpp
│   └── chatroom.cpp
├── data/
│   ├── users.txt
│   └── messages.txt
├── CMakeLists.txt
└── README.md
```
其中：
- `include/` 存放头文件
- `src/` 存放源文件
- `data/` 存放用户数据和聊天记录

---

## 核心类设计

### User
表示聊天室用户，保存用户名等基础信息。

### Message
表示一条聊天消息，包含发送者、接收者、消息内容等信息。

### ChatRoom
聊天室核心管理类，负责：
- 用户管理
- 消息管理
- 聊天记录加载与保存

---

## 编译与运行

### 环境要求
Linux
g++
CMake

### 编译
mkdir build
cd build
cmake ..
make

### 运行
./chatroom

### 使用 g++ 编译
g++ src/*.cpp -Iinclude -o chatroom
./chatroom

---

## 数据文件说明

- `data/users.txt`：保存用户信息
- `data/messages.txt`：保存聊天记录

程序启动时会读取已有数据，程序运行过程中会将更新后的数据保存到本地文件中。

---

## 示例功能流程

程序启动后，可通过菜单执行以下操作：

1. 添加用户
2. 发送消息
3. 查看消息记录
4. 查看用户列表
5. 删除用户
6. 清空消息
7. 退出程序

示例：

```text
===== ChatRoom Menu =====
1. Add User
2. Send Message
3. Show Messages
4. Show Users
5. Remove User
6. Clear Messages
0. Exit
```

---

## 第一阶段收获

通过本项目第一阶段，我主要练习了：

- C++ 基础语法
- STL 容器使用
- 面向对象设计
- 文件读写与数据持久化
- 基本工程结构组织
- Linux 下编译运行项目

这些内容为下一阶段的 TCP 聊天室开发打下了基础。

---

## 下一阶段计划

下一阶段将把当前单机聊天室升级为 TCP 聊天室，主要包括：

- client / server 架构
- 多客户端连接
- 消息广播
- 基础网络通信处理

进一步练习 Linux Socket 编程与网络开发能力。

---------------------------------------------------------------------------------------------------

# ChatRoom 01 第二阶段

---

## 第二阶段：TCP 多客户端聊天室

在第一阶段完成单机版聊天室后，第二阶段开始把项目升级为真正的网络聊天室。

这一阶段采用 **TCP client / server 架构**，客户端和服务端通过 Socket 建立连接，实现多个客户端同时在线聊天。项目重点从“本地模拟聊天逻辑”转向“网络通信 + 并发处理 + 广播机制”。

---

## 第二阶段目标

第二阶段的目标是实现一个基础的 TCP 多客户端聊天室，支持多个客户端连接到同一个服务端，并能够进行消息广播。

通过这一阶段，我主要练习：

- Linux Socket 编程基础
- TCP 通信流程
- client / server 架构
- 多线程处理多个客户端
- `std::mutex` 与共享资源保护
- 广播消息机制
- 命令行聊天室的双线程收发模型

---

## 第二阶段已实现功能

- 服务端启动并监听指定 IP 和端口
- 客户端连接服务端
- 服务端持续等待新的客户端连接
- 多个客户端可同时在线
- 每个客户端由独立线程处理通信
- 客户端消息广播给其他在线客户端
- 客户端支持持续发送和持续接收消息
- 用户连接后输入昵称
- 服务端记录客户端昵称
- 重名昵称校验
- 用户加入聊天室时广播系统消息
- 用户离开聊天室时广播系统消息
- 系统消息中显示当前在线人数

---

## 第二阶段项目结构

text
chatroom_01/
├── include/
│   ├── tcp_server.h
│   └── tcp_client.h
├── src/
│   ├── server_main.cpp
│   ├── client_main.cpp
│   ├── tcp_server.cpp
│   └── tcp_client.cpp
├── CMakeLists.txt
└── README.md

其中：
server_main.cpp：服务端程序入口
client_main.cpp：客户端程序入口
tcp_server.h / tcp_server.cpp：服务端核心逻辑
tcp_client.h / tcp_client.cpp：客户端核心逻辑

---

## 第二阶段核心设计

### TcpServer
第二阶段服务端核心类，负责：

创建监听 socket
绑定 IP 和端口
进入监听状态
接收客户端连接
为每个客户端创建处理线程
维护在线客户端列表
维护客户端昵称信息
广播聊天消息和系统消息

### TcpClient
第二阶段客户端核心类，负责：

连接服务端
发送消息
接收消息
主动断开连接

---

## 第二阶段编译与运行

### 环境要求
Linux
g++
CMake

### 编译
mkdir build
cd build
cmake ..
make

### 运行服务端
./server

### 运行客户端
./client

如果需要测试多人聊天，可以打开多个终端分别运行多个 client。

---

## 第二阶段使用流程
1. 启动服务端
先运行服务端程序：
./server
服务端启动后会监听指定地址和端口，等待客户端连接。

2. 启动客户端
在新的终端中运行客户端：
./client

3. 输入昵称
客户端连接成功后，需要先输入昵称，例如：
Enter your nickname: Alice
如果昵称没有重复，则进入聊天室。

4. 发送消息
进入聊天室后，可以持续输入消息，例如：
Enter message: hello
其他在线客户端会收到广播消息。

5. 退出聊天室
输入：
exit
客户端会主动断开连接，其他在线客户端会收到离线提示。

---

## 第二阶段示例效果

示例：
Enter your nickname: Alice
Enter message: hello
[System] Bob joined the chat. Online users: 2
[Bob] says: hi
[System] Bob left the chat. Online users: 1

如果输入重复昵称，客户端会收到类似提示：
[System] Nickname already taken. Please reconnect with another name.

---

## 第二阶段收获

通过本项目第二阶段，我主要练习了：
Linux Socket 网络编程基础
TCP 客户端 / 服务端通信模型
多线程处理多个客户端连接
共享资源加锁保护
广播机制设计
客户端双线程收发模型
聊天室昵称、上下线通知、在线人数等基础业务逻辑

这一阶段让我把第一阶段的聊天室业务思路，进一步迁移到了真实网络通信场景中。

---

## 第三阶段计划
第三阶段将继续在当前 TCP 聊天室基础上升级服务端处理模型，主要包括：

从“一个客户端一个线程”逐步过渡到 IO 多路复用
学习并实现 select
后续继续扩展到 poll / epoll
进一步理解高并发服务端的设计方式

这一阶段的重点将不再是补充基础聊天室功能，而是优化底层网络处理模型。