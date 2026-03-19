# ChatRoom 01

一个基于 C++ 实现的单机版聊天室项目，用于练习 Linux 环境下的 C++ 基础开发能力。

这是聊天室项目的第一阶段，当前版本采用本地模拟聊天方式，不涉及网络通信。项目重点在于练习面向对象设计、STL 容器、文件持久化以及工程结构组织，为后续 TCP 聊天室开发打基础。

---

## 项目目标

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
其中：
- `include/` 存放头文件
- `src/` 存放源文件
- `data/` 存放用户数据和聊天记录
```

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