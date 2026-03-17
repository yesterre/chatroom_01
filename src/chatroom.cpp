#include "chatroom.h"
#include <iostream>

void ChatRoom::addUser(const User& user){
    users.push_back(user);
}

void ChatRoom::sendMessage(const Message& msg){
    messages.push_back(msg);
}

void ChatRoom::showMessages() const {
    for (const auto& msg : messages)
    {
        std::cout << "User" <<msg.getSenderId()
                  <<"：" <<msg.getContent() 
                  <<"["<<msg.getTime()<<"]"
                  << std::endl;
    }
}