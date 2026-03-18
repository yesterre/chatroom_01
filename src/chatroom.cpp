#include "chatroom.h"
#include <iostream>
#include <fstream>
#include <sstream>  //字符串流，用来把一整行字符串当成“输入流”来处理

void ChatRoom::addUser(const User& user){
    users.push_back(user);    //把user添加到名为users的vector中
    saveUserToFile(user);    //把用户信息保存到文件中
}

void ChatRoom::sendMessage(const Message& msg){
    messages.push_back(msg);
    saveMessageToFile(msg);   //把消息保存到文件中
}

void ChatRoom::showMessages() const {
    //范围 for 循环
    //auto 代表编译器自动推断变量类型，即自动判断messages 里是 Message，auto=Message。& 代表引用，const 代表只读
    for (const auto& msg : messages)  //把 messages 里的每一条 Message 拿出来（不复制、不修改），依次叫做 msg 来使用
    {
        std::cout << getUserNameById(msg.getSenderId())
                  <<"：" <<msg.getContent() 
                  <<"["<<msg.getTime()<<"]"
                  << std::endl;                                                      
    }
}

bool ChatRoom::hasUser(int userId) const {
    for (const auto& user : users){
        if (user.getId() == userId){
            return true;
        }
    }
    return false;
}

std::string ChatRoom::getUserNameById(int userId) const {
    for (const auto& user : users){
        if (user.getId() == userId){
            return user.getName();
        }
    }
    return "Unknown";
}

void ChatRoom::showUsers() const {
    if (users.empty()){   //判断 users 这个 vector 是不是空的
        std::cout << "No users in the chatroom." << std::endl;
        return;
    }

    std::cout << "User list:" << std::endl;
    for (const auto& user : users){
        std::cout <<"ID:" << user.getId()
                  <<", Name:" << user.getName()
                  << std::endl;
    }
}

void ChatRoom::saveMessageToFile(const Message& msg) const {
    std::ofstream outfile("data/messages.txt",std::ios::app);   //打开一个输出文件流，把文件路径设为 data/messages.txt

    if (!outfile){
        std::cout << "Failed to open file: data/messages.txt" << std::endl;
        return;
    }

    outfile << msg.getSenderId() << "|" 
            << msg.getContent() << "|" 
            << msg.getTime() << std::endl;
}

void ChatRoom::loadMessagesFromFile() 
{
    std::ifstream infile("data/messages.txt",std::ios::app);

    if (!infile){
        std::cout << "FNo history file found. Starting with empty messages." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(infile, line)){
        std::stringstream ss(line);
        std::string senderIdstr;
        std::string content;
        std::string time;

        /*从字符串流 ss 里读取内容。
        第一句：读到 | 为止，得到发送者 id
        第二句：再读到 | 为止，得到消息内容
        第三句：读剩下的内容，得到时间
        */
        std::getline(ss, senderIdstr, '|');
        std::getline(ss, content, '|');
        std::getline(ss, time);

        int senderId = std::stoi(senderIdstr); //把字符串 senderIdstr 转换成整数 senderId，如把字符串 "1" 转成整数 1
        /*重新构造出一个消息对象，放进 messages*/
        Message msg(senderId, content, time);
        messages.push_back(msg);
    }
}

void ChatRoom::saveUserToFile(const User& user) const {
    std::ofstream outfile("data/users.txt", std::ios::app);

    if (!outfile) {
        std::cout << "Failed to open file: data/users.txt" << std::endl;
        return;
    }

    outfile << user.getId() << "|"
            << user.getName() << std::endl;
}

void ChatRoom::loadUsersFromFile() {
    std::ifstream infile("data/users.txt");

    if (!infile) {
        std::cout << "No user file found. Starting with empty users." << std::endl;
        return;
    }

    std::string line;
    while (std::getline(infile, line)) {
        std::stringstream ss(line);
        std::string idStr;
        std::string name;

        std::getline(ss, idStr, '|');
        std::getline(ss, name);

        int id = std::stoi(idStr);
        User user(id, name);
        users.push_back(user);
    }
}

