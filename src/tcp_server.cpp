/*
构造函数
析构函数
start()
run()

accept 一个客户端
进入循环
    recv 一次
    打印
    send 一次
直到客户端断开或出错
close
结束
*/

#include "tcp_server.h"

#include <arpa/inet.h>  //为了用inet_pton()和htons()，它们和 IP 地址、端口转换有关
#include <iostream>
#include <sys/socket.h>  ////为了用 socket 相关函数：socket()，bind()，listen()，accept()，send()，recv()
#include <unistd.h>  //为了用close()，在 Linux 下关闭文件描述符靠它
#include <cstring>  //为了用 memset()，在循环里清空缓冲区
#include <thread>

/*  构造函数
把传进来的 ip 保存到 ip_，把传进来的 port 保存到 port_，把 listen_fd_ 初始化成 -1
为什么用 -1？
因为在 Linux 里，合法文件描述符一般是非负数。所以 -1 常用来表示“当前还没有创建成功”。
*/
TcpServer::TcpServer(const std::string& ip, int port)
    : ip_(ip), port_(port), listen_fd_(-1) {}

/*  析构函数
对象销毁时，如果监听 socket 创建过，就把它关掉。
*/
TcpServer::~TcpServer() 
{
    if (listen_fd_ != -1) {
        close(listen_fd_);
    }
}

/*   start()：让服务端进入监听状态   */
bool TcpServer::start()
{
    /*创建socket：AF_INET：IPv4    SOCK_STREAM：TCP    0：让系统自动选择合适协议
    如果失败，返回值会小于 0   */
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    
    if(listen_fd_ < 0)
    {
        std::cerr << "socket() failed" << std::endl;
        return false;
    }

    /*构造一个 IPv4 地址结构体：sin_family：地址族，指定 IPv4    sin_port：端口号
    这里为什么要 htons(port_)？因为网络传输使用网络字节序，所以端口号要转换一下。所以写端口时一般要包一层 htons()！
    */
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    /*把 IP 字符串转换成网络地址：把"127.0.0.1"这种字符串 IP，转换成系统能识别的二进制地址格式。*/
    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) 
    {
        std::cerr << "inet_pton() failed" << std::endl;
        return false;
    }

    /*绑定地址和端口：把这个 socket 绑定到：指定 IP 和指定端口
    */
    if (bind(listen_fd_, 
            reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr)) < 0)
    {
        std::cerr << "bind() failed" << std::endl;
        return false;
    }

    /*开始监听：让这个 socket 进入监听状态，等待客户端连接。第二个参数 5 是 backlog，表示允许多少个客户端排队等待连接。
    */
    if (listen(listen_fd_, 5) < 0) 
    {
        std::cerr << "listen() failed" << std::endl;
        return false;
    }

    std::cout << "Server started at" << ip_ << ":" << port_ << std::endl;
    return true;
}

void TcpServer::handleClient(int client_fd, sockaddr_in client_addr) 
{
    char client_ip[INET_ADDRSTRLEN];  //INET_ADDRSTRLEN 是一个系统头文件里定义好的常量，表示IPv4 地址字符串表示所需要的最大长度
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);  //把 client_addr.sin_addr 里的 IPv4 地址，转换成字符串，写入 client_ip 数组中，数组长度是 INET_ADDRSTRLEN
    std::cout << "Client connected from " << client_ip 
              << ":" << ntohs(client_addr.sin_port) << std::endl;  //ntohs() 是把网络字节序的端口号转换成主机字节序，方便打印出来看

        //接收客户端消息：从客户端读数据到缓冲区里。这里先用一个简单字符数组缓冲区就够了
    char buffer[1024] = {0};

        while (true)   //内层循环：和当前这个客户端持续聊天
        {
            std::memset(buffer, 0, sizeof(buffer)); //每次循环前清空缓冲区，避免上次数据残留影响这次读取
            int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);  //recv() 返回实际收到的字节数，或者出错时返回 -1，或者客户端断开连接时返回 0

            if (bytes_received < 0) {  //recv() < 0：接收出错
                std::cerr << "recv() failed" << std::endl;
                break; //出错了，跳出循环，准备关闭连接
            } else if (bytes_received == 0) {  //recv() == 0：对端关闭连接了
                std::cout << "Client disconnected." << std::endl;
                break; //客户端断开了连接，跳出循环
            }

            //recv() > 0：成功收到数据
            buffer[bytes_received] = '\0'; //因为 recv() 收到的是字节数据，不保证自带字符串结束符，所以这里手动补一个 '\0'，便于按 C 风格字符串打印。
            std::cout << "[" << client_ip << ":" << ntohs(client_addr.sin_port)
                      << "] says: " << buffer << std::endl;

            std::string reply = "Message received by server.";  //回一条消息给客户端：给客户端发回确认消息
            int bytes_sent = send(client_fd, reply.c_str(), reply.size(), 0);
            if (bytes_sent < 0) {
                std::cerr << "send() failed" << std::endl;
                break; //出错了，跳出循环，准备关闭连接
            }
        }
        close(client_fd); //关闭和这个客户端的连接
}

/*  run()：接收一个客户端请求  */
void TcpServer::run()  //主线程只负责 accept，每接受一个客户端连接，就创建一个新线程来 handleClient，主线程继续等待下一个客户端连接
{
    while (true) 
    {
        /*给 accept() 用的，用来接收客户端地址信息。*/
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        /*接受一个客户端连接：等待一个客户端来连接，如果有客户端连上，返回一个新的文件描述符 client_fd
        注意：listen_fd_ 是监听用的，client_fd 是和具体客户端通信用的*/
        int client_fd = accept(listen_fd_, 
                                reinterpret_cast<sockaddr*>(&client_addr), 
                                &client_len);
        if (client_fd < 0)
        {
            std::cerr << "accept() failed" << std::endl;
            continue; //出错了，继续等待下一个客户端连接
        }

        std::thread client_thread(&TcpServer::handleClient, this, client_fd, client_addr);  //创建一个线程来处理这个客户端的通信
        client_thread.detach(); //分离线程，让它自己运行，主线程继续等待下一个客户端连接
        
    }
}