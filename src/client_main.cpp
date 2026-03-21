/*
只负责这几件事：
创建客户端对象
连接服务器
读取用户输入
发送消息
接收服务器回复并打印

目前是一个一次性客户端：
连上 server
发一条消息
收一条回复
结束程序
*/

#include "tcp_client.h"

#include <iostream>
#include <string>

int main(){
    TcpClient client("127.0.0.1",8888);  //与在 server_main.cpp 里设置的服务端地址必须一致

    if(!client.connectToServer()){  //尝试连接服务端
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;
    }

    std::string message;
    std::cout << "Enter message:";
    std::getline(std::cin, message);  //从终端读取一整行输入
                                      //用 getline 比 cin >> message 更合适，因为后面输入聊天消息时，可能会带空格，用 getline 才能完整读进来。

    if(!client.sendMessage(message)){   //把刚才输入的消息发给服务端
        std::cerr << "Failed to send message." << std::endl;
        return 1;
    }

    std::string reply = client.receiveMessage();   //等待服务端的回复，并把返回内容保存成字符串
    std::cout << "Server reply: " << reply << std::endl;  //打印服务端返回的内容

    return 0;
}