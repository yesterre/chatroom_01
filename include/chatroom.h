#ifndef CHATROOM_H
#define CHATROOM_H

#include <vector>
#include "user.h"
#include "message.h"

class ChatRoom {
private:
    std::vector<User> users;
    std::vector<Message> messages;

public:
    void addUser(const User& user);   //往聊天室添加一个用户
    void sendMessage(const Message& msg);  //用户发送一条消息（本质就是加入 messages）
    void showMessages() const;  //打印所有聊天记录
};

#endif // CHATROOM_H