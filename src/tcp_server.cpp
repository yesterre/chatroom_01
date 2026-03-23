#include "tcp_server.h"

#include <arpa/inet.h>  //为了用inet_pton()和htons()，它们和 IP 地址、端口转换有关
#include <iostream>
#include <sys/socket.h>  ////为了用 socket 相关函数：socket()，bind()，listen()，accept()，send()，recv(), setsockopt()
#include <unistd.h>  //为了用close()，在 Linux 下关闭文件描述符靠它
#include <algorithm> 

/*  构造函数
把传进来的 ip 保存到 ip_，把传进来的 port 保存到 port_，把 listen_fd_ 初始化成 -1
为什么用 -1？
因为在 Linux 里，合法文件描述符一般是非负数。所以 -1 常用来表示“当前还没有创建成功”。
*/
TcpServer::TcpServer(const std::string& ip, int port)
    : ip_(ip), port_(port), listen_fd_(-1), max_fd_(-1)
{
    FD_ZERO(&master_set_); //初始化 master_set_，把所有位都清零，表示当前没有任何文件描述符被监听
}

/*  析构函数
对象销毁时，如果监听 socket 创建过，就把它关掉。
*/
TcpServer::~TcpServer() 
{
    for (const auto& pair : clients_) { //对于 clients_ 里的每一个客户端命名为 pair，执行下面的代码
        close(pair.first); //把所有在线客户端的 socket 连接都关掉
    }

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

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //设置 socket 选项，允许地址重用，避免重启服务器时端口被占用的问题

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

    FD_SET(listen_fd_, &master_set_); //把监听 socket 的文件描述符加入到 master_set_ 里，表示我们要监听这个文件描述符的可读事件
    max_fd_ = listen_fd_; //更新 max_fd_，它表示当前 master_set_ 里最大的文件描述符值，select() 需要用到这个值来知道检查哪些文件描述符

    std::cout << "Server started at " << ip_ << ":" << port_ << std::endl;
    return true;
}

void TcpServer::handleNewConnection() 
{
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd_, 
                            reinterpret_cast<sockaddr*>(&client_addr), 
                            &client_len);
    if (client_fd < 0)
    {
        std::cerr << "accept() failed" << std::endl;
        return;
    }

    char client_ip[INET_ADDRSTRLEN];  //INET_ADDRSTRLEN 是一个系统头文件里定义好的常量，表示IPv4 地址字符串表示所需要的最大长度
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);  //把 client_addr.sin_addr 里的 IPv4 地址，转换成字符串，写入 client_ip 数组中，数组长度是 INET_ADDRSTRLEN
    
    std::cout << "Client connected from " << client_ip 
              << ":" << ntohs(client_addr.sin_port) 
              << ", fd = " << client_fd << std::endl;  //ntohs() 是把网络字节序的端口号转换成主机字节序，方便打印出来看

    clients_[client_fd] = ClientInfo{client_fd, "", false}; //把这个新客户端的文件描述符和一个空昵称关联起来，存到 clients_ 里，表示它在线了，但还没有设置昵称
    FD_SET(client_fd, &master_set_); //把这个新客户端的文件描述符加入到 master_set_ 里，表示我们要监听这个文件描述符的可读事件

    if (client_fd > max_fd_) { //更新 max_fd_，它表示当前 master_set_ 里最大的文件描述符值，select() 需要用到这个值来知道检查哪些文件描述符
        max_fd_ = client_fd;
    }
}

void TcpServer::handleClientMessage(int client_fd)
{
    //接收客户端消息：从客户端读数据到缓冲区里。这里先用一个简单字符数组缓冲区就够了
    char buffer[1024] = {0};
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);  //recv() 返回实际收到的字节数，或者出错时返回 -1，或者客户端断开连接时返回 0
    
    if (bytes_received < 0) 
    {  //recv() < 0：接收出错
        std::cerr << "recv() failed, fd = " << client_fd << std::endl;
        removeClient(client_fd); //从 clients_ 列表里移除这个客户端的文件描述符，表示它不在线了
        return; //出错了，直接返回，不继续处理这个客户端了
    }

    auto it = clients_.find(client_fd);
    if (it == clients_.end()) { //如果在 clients_ 里找不到这个客户端的文件描述符，说明这个客户端已经断开了连接了，或者根本就不在线了
        return; //直接返回，不继续处理这个客户端了
    }

    ClientInfo& client = it->second; //如果找到了这个客户端，就把它的信息取出来，命名为 client，方便后续使用

    if (bytes_received == 0) 
    { 
        std::string nickname = client.registered ? client.nickname : "Unknown"; //如果这个客户端之前注册过昵称，就用它的昵称，否则就用 "Unknown"
        
        removeClient(client_fd); //从 clients_ 列表里移除这个客户端的文件描述符，表示它不在线了

        if (nickname != "Unknown"){
            std::string leave_msg = 
                "[System] " + nickname + " left the chat. Online users: " + std::to_string(clients_.size());
            broadcastMessage(leave_msg, client_fd); //广播消息，告诉其他人这个人离开了
        }

        std::cout << "Client disconnected, fd = " << client_fd << std::endl;
        return; //客户端断开了连接，直接返回，不继续处理这个客户端了
    }
    
    buffer[bytes_received] = '\0'; 
    std::string text = buffer; //把收到的消息转换成 std::string，方便后续处理

    if (!client.registered) 
    {  //如果这个客户端还没有注册过昵称，就把它收到的第一条消息当作昵称来处理
        bool nickname_exists = false; //先假设这个昵称不存在，后面再检查一下
        for (const auto& pair : clients_) {  //对于 clients_ 里的每一个客户端命名为 pair，执行下面的代码
            if (pair.first != client_fd &&
                pair.second.registered &&
                pair.second.nickname == text) {
                nickname_exists = true;
                break;
            }
        }

        if (nickname_exists) {
            std::string error_message = "[System] Nickname already taken. Please reconnect with another name.";
            send(client_fd, error_message.c_str(), error_message.size(), 0); 

            removeClient(client_fd); //从 clients_ 列表里移除这个客户端的文件描述符，表示它不在线了
            return; //昵称重复了，直接返回，不继续处理这个客户端了
        }

        client.nickname = text; //把这个客户端的昵称设置成它收到的第一条消息
        client.registered = true; //把这个客户端标记成已经注册过昵称了

        std::cout << client.nickname << " joined the chat." << std::endl;

        std::string join_msg = 
            "[System] " + client.nickname + " joined the chat. Online users: " + std::to_string(clients_.size());
        broadcastMessage(join_msg, client_fd); //广播消息，告诉其他人这个人加入了
        return; //处理完昵称了，直接返回，不继续处理这个客户端了
    }

    std::string message = "[" + client.nickname + "] says: " + text; //把这个消息格式化一下，准备广播给其他人看
    
    std::cout << message << std::endl; //打印收到的消息，看看是什么内容
    broadcastMessage(message, client_fd); //广播消息，发给除了发送者之外的所有在线客户端
}


//遍历所有在线客户端，把消息发给除了发送者之外的每个人
void TcpServer::broadcastMessage(const std::string& message, int sender_fd)
{
    for (const auto& pair : clients_) {  //对于 clients_ 里的每一个客户端命名为 fd，执行下面的代码
        int fd = pair.first; //把这个客户端的文件描述符取出来，命名为 fd，方便后续使用
        if (fd == sender_fd) { //不发回给发送者自己
            continue; //发消息给这个客户端
        }

        int bytes_sent = send(fd, message.c_str(), message.size(), 0);
        if (bytes_sent < 0) {
            std::cerr << "broadcast send() failed, fd = " << fd << std::endl;
        }
    }
}

void TcpServer::removeClient(int client_fd)
{
    FD_CLR(client_fd, &master_set_); //从 master_set_ 里移除这个客户端的文件描述符，表示我们不再监听这个文件描述符的事件了
    close(client_fd); //把这个客户端的 socket 连接关掉

    clients_.erase(client_fd); //从 clients_ 中移除这个客户端的信息，表示它已经不在线了

    if (client_fd == max_fd_){
        max_fd_ = listen_fd_; //如果这个客户端的文件描述符是当前 master_set_ 里最大的，就把 max_fd_ 更新成 listen_fd_，因为 listen_fd_ 是我们一直在监听的
        for (const auto& pair : clients_) { //遍历 clients_ 里的每一个客户端命名为 fd，执行下面的代码
            if (pair.first > max_fd_) { //如果这个客户端的文件描述符比当前 max_fd_ 还大，就更新 max_fd_
                max_fd_ = pair.first;
            }
        }
    }
}

/*  run()：服务端主事件循环
   使用 select() 同时监听监听 socket 和所有已连接客户端 socket。
   - 如果 listen_fd_ 可读，说明有新连接到来，调用 handleNewConnection()
   - 如果某个 client_fd 可读，说明该客户端发来了消息或已断开，调用 handleClientMessage()  
*/
void TcpServer::run() 
{
    while (true) 
    {
        fd_set read_set = master_set_; //每次循环都要重新设置 read_set，因为 select() 会修改它

        int ready_count = select(max_fd_ + 1, &read_set, nullptr, nullptr, nullptr); //select() 返回准备好的文件描述符数量，或者出错时返回 -1
        if (ready_count < 0)
        {
            std::cerr << "select() failed" << std::endl;
            continue; //出错了，继续下一轮循环
        }

        for(int fd = 0; fd <= max_fd_; ++fd) {
            if(!FD_ISSET(fd, &read_set)) { //如果这个 fd 在本轮 select() 中不可读，就跳过
                continue;
            }

            if(fd == listen_fd_) { //如果这个文件描述符是监听 socket，说明有新的客户端连接请求了
                handleNewConnection(); //处理这个新的连接请求
            } else { //否则说明是某个客户端发来了消息
                handleClientMessage(fd); //处理这个客户端的消息
            }
        }
    }
}