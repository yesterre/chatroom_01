/*
只负责这几件事：
创建客户端对象
连接服务器
读取用户输入
发送消息
接收服务器回复并打印
*/

#include "tcp_client.h"

#include <iostream>
#include <string>
#include <atomic>
#include <thread>

int main()
{
    TcpClient client("127.0.0.1",8888);  //与在 server_main.cpp 里设置的服务端地址必须一致

    if(!client.connectToServer()){  //尝试连接服务端
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;
    }

    std::atomic<bool> running(true);  //定义一个原子布尔变量，表示客户端是否正在运行

    std::thread receive_thread([&](){
        while (running) {
            std::string message = client.receiveMessage();  //在一个单独的线程里不断等待服务端的消息

            if (message.empty()) {  //如果服务端断开连接或者消息为空，就退出循环
                std::cerr << "Server disconnected or recieve failed." << std::endl;
                running = false;  //把 running 设置为 false，通知主线程退出
                break;
            }

            std::cout << message << std::endl;  //打印服务端发来的消息
        }
    });


    std::string message;
    while(running)
    {
        std::cout << "Enter message:";
        std::getline(std::cin, message);  //从终端读取一整行输入
                                      //用 getline 比 cin >> message 更合适，因为后面输入聊天消息时，可能会带空格，用 getline 才能完整读进来。
        if(!running) 
        {  //如果 running 已经被设置为 false，说明服务端断开了连接，直接退出循环
            break;
        }
        
        if (message == "exit") 
        {
            running = false;  //如果用户输入 exit，就把 running 设置为 false，通知接收线程退出
            break;
        }

        if (message.empty()) 
        {  //如果用户输入了空消息，就提示一下，不发送给服务端
            std::cout << "Message cannot be empty. Please enter a valid message." << std::endl;
            continue;
        }

        if(!client.sendMessage(message))
        {   
            std::cerr << "Failed to send message." << std::endl;
            running = false;  //如果发送消息失败，说明服务端可能断开了连接，设置 running 为 false，通知接收线程退出
            break;
        }

    }
    receive_thread.join();  //等待接收线程结束
    return 0;
}