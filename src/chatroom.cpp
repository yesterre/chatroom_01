#include "chatroom.h"
#include <iostream>
#include <fstream>
#include <sstream>  //字符串流，用来把一整行字符串当成“输入流”来处理

bool ChatRoom::addUser(const User& user) {
    if (hasUser(user.getId())) {
        return false;
    }

    users.push_back(user);
    saveUserToFile(user);
    return true;
}


/*
users.begin()表示指向 users 第一个元素的位置。
users.end()表示指向“最后一个元素的下一个位置”。它不是最后一个元素本身，而是一个结束标记。
auto it = users.begin()意思是定义一个变量 it，让它一开始指向第一个用户。
这里的 auto 表示让编译器自动推断 it 的类型。
*/
//这一步删掉的是内存里的用户，但还没有更新 data/users.txt
bool ChatRoom::removeUser(int userId) {
    for (auto it = users.begin(); it != users.end(); ++it) 
    {   //it 是迭代器，可以先把它理解成“指向容器中某个元素的位置”
        if (it->getId() == userId) {
            users.erase(it);   //删除这个位置上的用户,erase 是 vector 的成员函数，参数是一个迭代器，表示要删除哪个位置上的元素
            rewriteUsersFile();
            return true;
        }
    }
    return false;
}

//重新读取，重写用户列表
/*没有 std::ios::app，所以它会覆盖写入。也就是先清空旧文件，再把当前 users 里的所有用户重新写进去*/
void ChatRoom::rewriteUsersFile() const {
    std::ofstream outfile("data/users.txt");  //创建一个叫 outfile 的文件输出流，并打开 data/users.txt 这个文件

    if (!outfile) {
        std::cout << "Failed to open file: data/users.txt" << std::endl;
        return;
    }

    for (const auto& user : users) {
        outfile << user.getId() << "|"
                << user.getName() << std::endl;
    }
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

void ChatRoom::clearMessages(){
    messages.clear();   //清空内存里的消息列表,vector的成员函数clear

    std::ofstream outfile("data/messages.txt");  //打开一个输出文件流，文件路径是 data/messages.txt
    if (!outfile){
        std::cout << "Failed to open file: data/messages.txt" << std::endl;
        return;
    }

    outfile.close();
    std::cout << "All messages have been cleared." << std::endl;
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
        if (!hasUser(id)) {
            User user(id, name);
            users.push_back(user);
        }
    }
}

