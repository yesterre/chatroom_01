/*
表示一个最小 TCP 客户端，先只做三件事：
连接服务器
发送一条消息
接收一条回复
*/

#pragma once

#include <string>

class TcpClient
{
    public:
        TcpClient(const std::string& ip, int port);  //创建客户端对象时，指定它要连接的目标地址和端口
        ~TcpClient();  //客户端对象销毁时，关闭 socket 文件描述符，避免资源泄漏

        bool connectToServer();  //发起连接请求，成功返回 true，失败返回 false
        bool sendMessage(const std::string& message);  //把一条字符串消息发给服务器
        std::string receiveMessage();  //从服务器接收回复内容
        void disconnect();  //主动断开与服务器的连接

    private:
        std::string ip_;  //保存服务器 IP 地址
        int port_;        //保存服务器端口号
        int sock_fd_;     //保存客户端 socket 文件描述符。
                          // 它和服务端的 listen_fd_ 不一样：服务端 listen_fd_：监听连接用;客户端 sock_fd_：直接和服务端通信用
};