#include "tcp_server.h"

#include <arpa/inet.h>  //为了用inet_pton()和htons()，它们和 IP 地址、端口转换有关
#include <sys/socket.h>  //为了用 socket()、bind()、listen()、accept()、recv()、send() 等函数，这些都是 socket 编程的基础函数
#include <netinet/in.h>  //为了用 sockaddr_in 结构体，这个结构体用来表示 IPv4 地址和端口
#include <cstring> 
#include <iostream>
#include <unistd.h>  //为了用close()，在 Linux 下关闭文件描述符靠它

/*  构造函数
把传进来的 ip 保存到 ip_，把传进来的 port 保存到 port_，把 listen_fd_ 初始化成 -1
为什么用 -1？
因为在 Linux 里，合法文件描述符一般是非负数。所以 -1 常用来表示“当前还没有创建成功”。
*/
TcpServer::TcpServer(const std::string& ip, int port)
    : ip_(ip), port_(port), listen_fd_(-1), epoll_fd_(-1)
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

    if (epoll_fd_ != -1) 
    { //如果 epoll 实例创建过，就把它关掉  epoll 实例本身也是一个 fd
        close(epoll_fd_);
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

    /*可以把它想成创建一个“事件管理器”。
    以后这个管理器就负责帮你记住：你关心哪些 fd  哪些 fd 发生了事件*/
    epoll_fd_ = epoll_create1(0); //创建 epoll 实例，参数 0 表示默认选项，如果失败了，返回值会小于 0
    if (epoll_fd_ < 0)
    {
        std::cerr << "epoll_create1() failed" << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (!addEpollFd(listen_fd_)) //把监听 socket 的文件描述符加入到 epoll 实例里，表示我们要监听这个文件描述符的事件，如果失败了，返回 false
    {
        close(epoll_fd_);
        epoll_fd_ = -1;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    std::cout << "Server started at " << ip_ << ":" << port_ << std::endl;
    return true;
}

bool TcpServer::addEpollFd(int fd)  //把这个文件描述符加入到 epoll 实例里，表示我们要监听这个文件描述符的事件
{
    epoll_event ev{};  //定义一个 epoll_event 结构体变量 ev，初始化成全零
    ev.events = EPOLLIN;  //把 ev 的 events 字段设置成 EPOLLIN，表示我们要监听这个文件描述符的可读事件
    ev.data.fd = fd;  //把 ev 的 data 字段的 fd 子字段设置成这个文件描述符，表示我们要监听这个文件描述符的事件时，关联它的数据就是这个文件描述符本身

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0)  //把这个 fd 加入 epoll 的关注列表
    {
        std::cerr << "epoll_ctl ADD failed, fd = " << fd << std::endl;
        return false;
    }

    return true;
}

bool TcpServer::removeEpollFd(int fd) 
{
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0)
    {
        std::cerr << "epoll_ctl DEL failed, fd = " << fd << std::endl;
        return false;
    }

    return true;
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

    addEpollFd(client_fd); //把这个新连接的文件描述符加入到 epoll 实例里，表示我们要监听这个文件描述符的事件

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
    removeEpollFd(client_fd); //先把这个客户端的文件描述符从 pollfd 列表里移除掉，表示我们不再监听这个文件描述符的事件了
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
        int ready_count = epoll_wait(epoll_fd_, events_, MAX_EVENTS, -1); 
        //epoll_wait() 会阻塞等待，直到有事件发生或者出错了。它会把发生事件的文件描述符的信息写入 events_ 数组里，最多写 MAX_EVENTS 个。返回值是实际发生事件的数量，或者出错时返回 -1。
        if (ready_count < 0)
        {
            std::cerr << "epoll_wait() failed" << std::endl;
            continue; //出错了，继续下一轮循环，重新等待事件发生，不要直接退出程序，因为可能只是暂时的错误，比如被信号打断了。
        }

        for (int i = 0; i < ready_count; ++i)  //遍历的是 ready_count，不是所有客户端
        {
            int current_fd = events_[i].data.fd;  //拿出这条就绪事件对应的是哪个 fd

            if (!(events_[i].events & EPOLLIN))  //只关心可读
            {
                continue;
            }

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