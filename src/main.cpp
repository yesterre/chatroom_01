#include <iostream>
#include "user.h"
#include "message.h"

int main() {
    User user1(1, "Alice");

    Message msg(1, "Hello everyone!", "2026-03-16");

    std::cout << "sender id: " << msg.getSenderId() << std::endl;
    std::cout << "content: " << msg.getContent() << std::endl;
    std::cout << "time: " << msg.getTime() << std::endl;

    return 0;
}