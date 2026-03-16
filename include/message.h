#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>

class Message{
private:
    int senderId;
    std::string content;
    std::string time;

public:
    Message();
    Message(int senderId, const std::string& content, const std::string& time);

    int getSenderId() const;
    std::string getContent() const;
    std::string getTime() const;
};

#endif