#include "chatroom.h"
#include <iostream>

void ChatRoom::addUser(const User& user){
    users.push_back(user);    //把user添加到名为users的vector中
}

void ChatRoom::sendMessage(const Message& msg){
    messages.push_back(msg);
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