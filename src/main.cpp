#include <iostream>
#include <string>
#include "chatroom.h"

int main() {
    ChatRoom room;
    int choice;

    while(true)
    {
        std::cout <<"\n===== ChatRoom Menu =====" << std::endl;
        std::cout << "1. Add User" << std::endl;
        std::cout << "2. Send Message" << std::endl;
        std::cout << "3. Show Messages" << std::endl;
        std::cout << "4. Exit" << std::endl;
        std::cout << "Enter your choice: ";
        std::cin >> choice;
    }

    if(choice == 1)
    {
        int id;
        std::string name;

        std::cout << "Enter user ID: ";
        std::cin >> id;
        std::cout << "Enter user name: ";
        std::cin >> name;

        User user(id, name);
        room.addUser(user);

        std::cout << "User added successfully!" << std::endl;
    }
    else if (choice == 2)
    {
        int senderId;
        std::string content;
        std::string time;

        std::cout << "Enter sender ID: ";
        std::cin >> senderId;

        if (!room.hasUser(senderId)) {
            std::cout << "User does not exist." << std::endl;
            continue;
        }

        std::cin.ignore(); // Clear the input buffer
        std::cout << "Enter message content: ";
        std::getline(std::cin, content);

        std::cout << "Enter message time: ";
        std::getline(std::cin, time);

        Message msg(senderId, content, time);
        room.sendMessage(msg);

        std::cout << "Message sent successfully!" << std::endl;
    }
    else if (choice == 3)
    {
        room.showMessages();
    }
    else if (choice == 4)
    {
        std::cout << "Bye!" << std::endl;
        break;
    }
    else
    {
        std::out << "Invalid choice. Please try again." << std::endl;
    }

    return 0;
}