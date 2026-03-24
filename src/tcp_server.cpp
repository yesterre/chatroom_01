#include "tcp_server.h"

#include <arpa/inet.h>  //为了用inet_pton()和htons()，它们和 IP 地址、端口转换有关
#include <cstring>  //为了用 memset()，它可以把一块内存区域设置成全零或者全某个值
#include <iostream>
#include <unistd.h>  //为了用close()，在 Linux 下关闭文件描述符靠它

/*  构造函数
把传进来的 ip 保存到 ip_，把传进来的 port 保存到 port_，把 listen_fd_ 初始化成 -1
为什么用 -1？
因为在 Linux 里，合法文件描述符一般是非负数。所以 -1 常用来表示“当前还没有创建成功”。
*/
TcpServer::TcpServer(const std::string& ip, int port)
    : ip_(ip), port_(port), listen_fd_(-1)
{
}

/*  析构函数
对象销毁时，如果监听 socket 创建过，就把它关掉。
*/
TcpServer::~TcpServer() 
{
    for (auto& pair : clients_) 
    { //对于 clients_ 里的每一个客户端命名为 pair，执行下面的代码
        close(pair.first); //把所有在线客户端的 socket 连接都关掉
    }

    if (listen_fd_ != -1) 
    { //如果监听 socket 创建过，就把它关掉
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
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)   //设置 socket 选项：SO_REUSEADDR 允许我们在同一端口上快速重启服务器，避免“Address already in use”错误
    {
        std::cerr << "setsockopt() failed" << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    /*构造一个 IPv4 地址结构体：sin_family：地址族，指定 IPv4    sin_port：端口号
    这里为什么要 htons(port_)？因为网络传输使用网络字节序，所以端口号要转换一下。所以写端口时一般要包一层 htons()！
    */
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    server_addr.sin_addr.s_addr = inet_addr(ip_.c_str()); //把 IP 字符串转换成网络字节序的整数，直接赋值给 sin_addr.s_addr

    /*绑定地址和端口：把这个 socket 绑定到：指定 IP 和指定端口
    */
    if (bind(listen_fd_, 
            (sockaddr*)&server_addr,
            sizeof(server_addr)) < 0)
    {
        std::cerr << "bind() failed" << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    /*开始监听：让这个 socket 进入监听状态，等待客户端连接。第二个参数 5 是 backlog，表示允许多少个客户端排队等待连接。
    */
    if (listen(listen_fd_, 10) < 0) 
    {
        std::cerr << "listen() failed" << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    addPollfd(listen_fd_); //把监听 socket 的文件描述符加入到 pollfd 列表里，表示我们要监听这个文件描述符的事件

    std::cout << "Server started at " << ip_ << ":" << port_ << std::endl;
    return true;
}

void TcpServer::addPollfd(int fd) 
{
    struct pollfd pfd;
    pfd.fd = fd; //把这个文件描述符设置成我们要监听的那个
    pfd.events = POLLIN; //我们要监听这个文件描述符的可读事件   监听 fd 可读：说明有新连接   客户端 fd 可读：说明有消息或者断开
    pfd.revents = 0; //这个字段是用来存放 poll() 返回时告诉我们哪个事件发生了的，初始化成 0 就行了

    poll_fds_.push_back(pfd); //把这个 pollfd 结构体加入到 poll_fds_ 列表里，表示我们要监听这个文件描述符的事件
}

int TcpServer::findPollfdIndex(int fd) const
{
    for (size_t i = 0; i < poll_fds_.size(); ++i) 
    { //遍历 poll_fds_ 列表里的每一个 pollfd 结构体，命名为 pfd，执行下面的代码
        if (poll_fds_[i].fd == fd) 
        { //如果这个 pollfd 结构体的 fd 字段等于我们要找的那个文件描述符，就返回它在列表里的索引 i
            return static_cast<int>(i);
        }
    }
    return -1; //如果找不到，就返回 -1，表示没有找到
}

void TcpServer::removePollfd(int fd) 
{
    int index = findPollfdIndex(fd); //先找到这个文件描述符在 poll_fds_ 列表里的索引
    if (index != -1) 
    { //如果找到了，就把它从列表里移除掉
        poll_fds_.erase(poll_fds_.begin() + index); //把这个索引位置的 pollfd 结构体从列表里移除掉
    }
}

void TcpServer::handleNewConnection() 
{
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(listen_fd_, 
                            (sockaddr*)&client_addr, 
                            &client_len);  //accept() 返回一个新的文件描述符，代表这个新连接的 socket 连接，如果失败了，返回值会小于 0
    if (client_fd < 0)
    {
        std::cerr << "accept() failed" << std::endl;
        return;
    }

    addPollfd(client_fd); //把这个新连接的文件描述符加入到 pollfd 列表里，表示我们要监听这个文件描述符的事件

    char client_ip[INET_ADDRSTRLEN];  //INET_ADDRSTRLEN 是一个系统头文件里定义好的常量，表示IPv4 地址字符串表示所需要的最大长度
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);  //把 client_addr.sin_addr 里的 IPv4 地址，转换成字符串，写入 client_ip 数组中，数组长度是 INET_ADDRSTRLEN
    
    std::cout << "Client connected from " << client_ip 
              << ":" << ntohs(client_addr.sin_port) 
              << ", fd = " << client_fd << std::endl;  //ntohs() 是把网络字节序的端口号转换成主机字节序，方便打印出来看

    clients_[client_fd] = ClientInfo{client_fd, "", false}; //把这个新客户端的文件描述符和一个空昵称关联起来，存到 clients_ 里，表示它在线了，但还没有设置昵称
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

    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) //如果这个消息的最后一个字符是换行符或者回车符，就把它去掉，避免后续处理时出现问题
    {
        text.pop_back();
    }

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
            sendToClient(client_fd, error_message);

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

void TcpServer::sendToClient(int client_fd, const std::string& message)
{
    int bytes_sent = send(client_fd, message.c_str(), message.size(), 0);
    if (bytes_sent < 0) {
        std::cerr << "send() failed, fd = " << client_fd << std::endl;
    }
}

void TcpServer::removeClient(int client_fd)
{
    removePollfd(client_fd); //先把这个客户端的文件描述符从 pollfd 列表里移除掉，表示我们不再监听这个文件描述符的事件了
    close(client_fd); //把这个客户端的 socket 连接关掉，释放系统资源

    clients_.erase(client_fd); //从 clients_ 中移除这个客户端的信息，表示它已经不在线了

    clients_.erase(client_fd); // 从客户端状态表中移除
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
        int ready_count = poll(poll_fds_.data(), poll_fds_.size(), -1); 
        //调用 poll() 来等待事件发生，data 是 vector 里的数组首地址，size 告诉它一共要监控多少个 fd，以及 -1 表示无限等待
        if (ready_count < 0)
        {
            std::cerr << "poll() failed" << std::endl;
            break; //出错了，直接跳出循环，结束服务器运行
        }

        size_t current_size = poll_fds_.size(); //把当前 poll_fds_ 列表的大小保存到 current_size 里，方便后续使用

        for (size_t i = 0; i < current_size; ++i) //遍历 poll_fds_ 列表里的每一个 pollfd 结构体，命名为 pfd，执行下面的代码
        {
            if (i >= poll_fds_.size())
            {
                break;
            }

            if (!(poll_fds_[i].revents & POLLIN))  //如果这一项没有发生 POLLIN 事件  那就跳过，不处理它
            {
                continue;
            }

            int current_fd = poll_fds_[i].fd;

            //监听 fd 代表新连接、客户端 fd 代表消息
            if (current_fd == listen_fd_)
            {
                handleNewConnection();
            }
            else
            {
                handleClientMessage(current_fd);
            }
        }
    }
}