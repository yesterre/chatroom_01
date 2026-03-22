/*
表示一个最小 TCP 服务端。
它先只负责：
记住监听地址和端口
创建监听 socket
启动服务
跑主循环，接收一个客户端消息
*/

#pragma once   //防止头文件被重复包含

#include <string>
#include <vector>
#include <mutex>
#include <netinet/in.h>  //包含 sockaddr_in 结构体定义
#include <unordered_map>

class TcpServer
{
    public:
        TcpServer(const std::string& ip, int port);   //创建服务端对象时指定：监听的 IP 和监听的端口
        ~TcpServer();  //对象销毁时做清理工作。后面主要用于关闭 socket 文件描述符，属于Linux 网络编程里的资源释放

        bool start();  //启动服务端，主要是创建监听 socket，绑定地址和端口，进入监听状态
        void run();  //服务端进入运行状态

    private:
        void handleClient(int client_fd, sockaddr_in client_addr);  //处理客户端连接的函数，参数是客户端的 socket 文件描述符和客户端地址信息
        void broadcastMessage(const std::string& message, int sender_fd);  //向所有连接的客户端广播消息，参数是要广播的消息和发送者的 socket 文件描述符（可以用来排除发送者自己）
        void removeClient(int client_fd);  //从客户端列表中移除一个客户端连接，参数是客户端的 socket 文件描述符

        std::string ip_;  //保存服务端监听的 IP 地址。加下划线 _ 是常见成员变量命名习惯，表示它是类的内部成员。
        int port_;  //保存端口号
        int listen_fd_;  
        //监听 socket 的文件描述符。在 Linux 里，socket 也是文件描述符的一种，所以通常用 int 保存

        std::unordered_map<int, std::string> nicknames_;  //保存客户端的昵称，键是客户端的 socket 文件描述符，值是对应的昵称。使用 unordered_map 是因为它提供了快速的查找和插入操作
        std::vector<int> clients_;  //保存当前连接的客户端 socket 文件描述符列表。使用 vector 是因为客户端数量不固定，可以动态增加和减少
        std::mutex clients_mutex_;  //保护 clients_ 列表的互斥锁，确保在多线程环境下对 clients_ 的访问是安全的
};