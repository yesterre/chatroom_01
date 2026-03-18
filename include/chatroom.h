#ifndef CHATROOM_H
#define CHATROOM_H

#include <vector>
#include <string>
#include "user.h"
#include "message.h"

class ChatRoom {
private:
    std::vector<User> users;  //定义了一个名字叫 users 的容器，里面可以存很多个 User 对象
    std::vector<Message> messages;

public:
    //void addUser(const User& user);   //往聊天室添加一个用户
    bool addUser(const User& user);
    bool removeUser(int userId);  //从聊天室里删除一个用户（通过用户 ID 来删除）
    void sendMessage(const Message& msg);  //用户发送一条消息（本质就是加入 messages）
    void showMessages() const;  //打印所有聊天记录
    void showUsers() const;  //打印聊天室里所有用户的名字
    bool hasUser(int userId) const; //检查聊天室里是否有某个用户（通过用户 ID 来判断）
    std::string getUserNameById(int userId) const; //通过用户 ID 获取用户名
    
    void saveMessageToFile(const Message& msg) const; //把聊天记录保存到一个文本文件里
    void loadMessagesFromFile(); //从文本文件里加载聊天记录
    
    void saveUserToFile(const User& user) const; //把用户信息保存到一个文本文件里
    void loadUsersFromFile(); //从文本文件里加载用户信息
    void rewriteUsersFile() const;  //重写用户文件（当用户被删除时需要调用这个函数来更新用户文件）
};

#endif // CHATROOM_H