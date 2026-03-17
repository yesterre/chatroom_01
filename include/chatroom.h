#ifndef CHATROOM_H
#define CHATROOM_H

#include <vector>
#include "user.h"
#include "message.h"

class ChatRoom {
private:
    std::vector<User> users;  //定义了一个名字叫 users 的容器，里面可以存很多个 User 对象
    std::vector<Message> messages;

public:
    void addUser(const User& user);   //往聊天室添加一个用户
    void sendMessage(const Message& msg);  //用户发送一条消息（本质就是加入 messages）
    void showMessages() const;  //打印所有聊天记录
    bool hasUser(int userId) const; //检查聊天室里是否有某个用户（通过用户 ID 来判断）
};

#endif // CHATROOM_H