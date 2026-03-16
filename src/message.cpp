#include "message.h"

Message::Message(){
    senderId = 0;
    content = "";
    time = "";
}

Message::Message(int senderId, std::string content, std::string time){
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