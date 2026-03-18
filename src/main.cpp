#include <iostream>
#include <string>
#include "chatroom.h"
#include <ctime>
#include <iomanip>

std::string getCurrentTime() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);

    return std::string(buffer);
}

int main() {
    ChatRoom room;
    room.loadUsersFromFile();
    room.loadMessagesFromFile();
    int choice;

    while(true)
    {
        std::cout <<"\n===== ChatRoom Menu =====" << std::endl;
        std::cout << "1. Add User" << std::endl;
        std::cout << "2. Send Message" << std::endl;
        std::cout << "3. Show Messages" << std::endl;
        std::cout << "4. Show Users" << std::endl;
        std::cout << "5. Remove User" << std::endl;
        std::cout << "6. Clear Messages" << std::endl;
        std::cout << "7. Exit" << std::endl;
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        if(choice == 1)
        {
            int id;
            std::string name;

            std::cout << "Enter user ID: ";
            std::cin >> id;
            std::cout << "Enter user name: ";
            std::cin >> name;

            User user(id, name);
            if (room.addUser(user)) {
                std::cout << "User added successfully!" << std::endl;
            } else {
                std::cout << "User ID already exists. Add user failed." << std::endl;
            }
        }
        else if (choice == 2)
        {
            int senderId;
            std::string content;

            std::cout << "Enter sender ID: ";
            std::cin >> senderId;

            if (!room.hasUser(senderId)) {
                std::cout << "User does not exist." << std::endl;
                continue;
            }

            std::cin.ignore(); // Clear the input buffer
            std::cout << "Enter message content: ";
            std::getline(std::cin, content);

            std::string time = getCurrentTime();

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
            room.showUsers();
        }
        else if (choice == 5)
        {
            int userId;
            std::cout << "Enter user ID to remove: ";
            std::cin >> userId;

            if (room.removeUser(userId)) {
                std::cout << "User removed successfully!" << std::endl;
            } else {
                std::cout << "User ID not found. Remove user failed." << std::endl;
            }
        }
        else if (choice == 6)
        {
            room.clearMessages();
        }
        else if (choice == 7)
        {
            std::cout << "Bye!" << std::endl;
            break;
        }
        else
        {
            std::cout << "Invalid choice. Please try again." << std::endl;
        }
    }
    return 0;
}