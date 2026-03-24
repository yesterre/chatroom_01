#pragma once   //防止头文件被重复包含


#include <string>
#include <vector>  //包含 vector 容器定义
#include <poll.h>  //包含 poll 函数和相关结构体定义
#include <sys/socket.h>  //包含 socket 函数和相关结构体定义
#include <netinet/in.h>  //包含 sockaddr_in 结构体定义
#include <unordered_map>  //包含 unordered_map 容器定义

class TcpServer
{
    public:
        TcpServer(const std::string& ip, int port);   //创建服务端对象时指定：监听的 IP 和监听的端口
        ~TcpServer();  //对象销毁时做清理工作。后面主要用于关闭 socket 文件描述符，属于Linux 网络编程里的资源释放

        bool start();  //启动服务端，主要是创建监听 socket，绑定地址和端口，进入监听状态
        void run();  //服务端进入运行状态

    private:
        void handleNewConnection();  
        void handleClientMessage(int client_fd);  //处理客户端发送的消息，参数是客户端的 socket 文件描述符
        void sendToClient(int client_fd, const std::string& message);  //向指定客户端发送消息，参数是客户端的 socket 文件描述符和要发送的消息内容
        void broadcastMessage(const std::string& message, int sender_fd);  //向所有连接的客户端广播消息，参数是要广播的消息和发送者的 socket 文件描述符（可以用来排除发送者自己）
        void removeClient(int client_fd);  //从客户端列表中移除一个客户端连接，参数是客户端的 socket 文件描述符

        void addPollfd(int fd);  //将一个文件描述符添加到 poll 监视列表中，参数是要添加的文件描述符
        void removePollfd(int fd);  //从 poll 监视列表中移除一个文件描述符，参数是要移除的文件描述符
        int findPollfdIndex(int fd) const;  //在 poll 监视列表中查找一个文件描述符，参数是要查找的文件描述符，返回值是该文件描述符在 poll 监视列表中的索引，如果没有找到则返回 -1

    private:
        std::string ip_;  //保存服务端监听的 IP 地址。加下划线 _ 是常见成员变量命名习惯，表示它是类的内部成员。
        int port_;  //保存端口号
        int listen_fd_;  
        //监听 socket 的文件描述符。在 Linux 里，socket 也是文件描述符的一种，所以通常用 int 保存

        struct ClientInfo
        {
            int fd;  //客户端的 socket 文件描述符
            std::string nickname;  //客户端的昵称
            bool registered;  //客户端是否已经注册了昵称
        };

        std::unordered_map<int, ClientInfo> clients_;  //保存所有在线客户端的信息，键是客户端 fd，值是该客户端对应的状态信息
        std::vector<struct pollfd> poll_fds_;  //保存所有需要监视的文件描述符，包括监听 socket 和所有客户端 socket

        /*
        struct pollfd 是 Linux poll 机制里专门用的一个结构体类型。
        struct pollfd
        {
            int fd;         // 文件描述符
            short events;   // 你关心它发生什么事件
            short revents;  // 实际发生了什么事件
        };
        */
        
};