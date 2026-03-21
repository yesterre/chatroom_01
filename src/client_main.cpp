/*
只负责这几件事：
创建客户端对象
连接服务器
读取用户输入
发送消息
接收服务器回复并打印

目前是一个可持续客户端：
连接服务器
进入循环
    输入一条
    发送一条
    接收一条
直到用户主动退出
*/

#include "tcp_client.h"

#include <iostream>
#include <string>

int main()
{
    TcpClient client("127.0.0.1",8888);  //与在 server_main.cpp 里设置的服务端地址必须一致

    if(!client.connectToServer()){  //尝试连接服务端
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;
    }

    std::string message;
    while(true)
    {
        std::cout << "Enter message:";
        std::getline(std::cin, message);  //从终端读取一整行输入
                                      //用 getline 比 cin >> message 更合适，因为后面输入聊天消息时，可能会带空格，用 getline 才能完整读进来。
        if (message == "exit") {
            break;
        }

        if(!client.sendMessage(message))
        {   //把刚才输入的消息发给服务端
            std::cerr << "Failed to send message." << std::endl;
            break;
        }

        std::string reply = client.receiveMessage();   //等待服务端的回复，并把返回内容保存成字符串
        if (reply.empty()) {  //reply.empty() 是 std::string 的成员函数，用来判断字符串是否为空
            std::cerr << "Server disconnected or reply is empty." << std::endl;
            break;
        }
        std::cout << "Server reply: " << reply << std::endl;  //打印服务端返回的内容
    }
    return 0;
}