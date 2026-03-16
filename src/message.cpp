#include "message.h"

Message::Message(){
    senderId = 0;
    content = "";
    time = "";
}

Message::Message(int senderId, const std::string& content, const std::string& time){   //创建 Message 对象
    this->senderId = senderId;
    this->content = content;
    this->time = time;
}

int Message::getSenderId() const {
    return senderId;
}

std::string Message::getContent() const {
    return content;
}

std::string Message::getTime() const {
    return time;
}