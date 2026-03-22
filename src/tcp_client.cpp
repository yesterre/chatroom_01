#include "tcp_client.h"

#include <arpa/inet.h>  //处理 IP 地址转换
#include <cstring>
#include <iostream>
#include <unistd.h>  //Linux/Unix 下常见的系统调用，用于关闭文件描述符
#include <sys/socket.h>  //socket 编程最核心的头文件之一 connect()、send()、recv() 等函数都在这里声明

/*
构造函数的特点是：
名字和类名相同
没有返回值类型
对象创建时自动调用
*/
TcpClient::TcpClient(const std::string& ip, int port)
    : ip_(ip), port_(port), sock_fd_(-1) {}

/*
析构函数的特点：
名字是 ~类名
没有返回值
对象销毁时自动调用
*/
TcpClient::~TcpClient() {
    if (sock_fd_ != -1) {  //判断这个 socket 是否有效
        close(sock_fd_);  //关闭套接字文件描述符，释放资源
    }
}


bool TcpClient::connectToServer() {
    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);  //创建一个 socket，AF_INET 表示 IPv4，SOCK_STREAM 表示 TCP 协议
    if (sock_fd_ < 0) {
        std::cerr << "socket() failed" << std::endl;
        return false;
    }

    //端口号放进地址结构前，通常要 htons
    //IP 地址转换一般用 inet_pton
    sockaddr_in server_addr{};  //定义一个 sockaddr_in 结构体来存储服务器地址信息，{} 表示值初始化，通常会把成员清零
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    //ip_.c_str() 把 std::string 转成 C 风格字符串
    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {  //inet_pton 将 IP 地址从文本转换为二进制形式
        std::cerr << "inet_pton() failed" << std::endl;
        return false;
    }

    //拿着这个 socket，去连接指定服务器地址，如果使用 sock_fd_ 去连接 server_addr 失败，就进入错误处理
    //reinterpret_cast是 C++ 里的强制类型转换方式之一，表示我们明确知道这个转换是安全的
    if (connect(sock_fd_,   //刚创建好的 socket
                reinterpret_cast<sockaddr*>(&server_addr), //connect() 函数需要一个 sockaddr* 类型的地址参数，所以我们需要把 sockaddr_in* 转换成 sockaddr*
                sizeof(server_addr)) < 0) {
        std::cerr << "connect() failed" << std::endl;
        return false;
    }

    std::cout << "Connected to server at " << ip_ << ":" << port_ << std::endl;
    return true;
}

bool TcpClient::sendMessage(const std::string& message) {
    if (sock_fd_ < 0) {
        std::cerr << "socket is not connected" << std::endl;
        return false;
    }

    //sock_fd_：往哪个 socket 发  message.c_str()：发送的数据起始地址  message.size()：发送多少字节  0：标志位
    int bytes_sent = send(sock_fd_, message.c_str(), message.size(), 0);  //send() 是 C 风格接口
    if (bytes_sent < 0) {
        std::cerr << "send() failed" << std::endl;
        return false;  
    }

    return true;
}

//std::string : 这个函数调用后，直接拿到“收到的文本消息”
std::string TcpClient::receiveMessage() {
    if (sock_fd_ < 0) {
        std::cerr << "socket is not connected" << std::endl;
        return "";  //一旦 recv() 出错，就会返回空字符串 ""
    }

    char buffer[1024] = {0};
    //sock_fd_：从哪个 socket 收  buffer：收到的数据放哪里  sizeof(buffer) - 1：最多接收多少字节  0：标志位
    int bytes_received = recv(sock_fd_, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "recv() failed" << std::endl;
        return "";
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received message
    return std::string(buffer);
}

void TcpClient::disconnect() {
    if (sock_fd_ != -1) {
        shutdown(sock_fd_, SHUT_RDWR);  //表示主动关闭这个 socket 的收发方向
        close(sock_fd_);
        sock_fd_ = -1;
    }
}