#include <iostream>
#include "chatroom.h"

int main() {
    ChatRoom room;

    User u1(1, "Alice");
    User u2(2, "Bob");

    room.addUser(u1);
    room.addUser(u2);

    Message m1(1, "Hello!", "2026-03-17");
    Message m2(2, "Hi Alice!", "2026-03-17");

    room.sendMessage(m1);
    room.sendMessage(m2);

    room.showMessages();

    return 0;
}