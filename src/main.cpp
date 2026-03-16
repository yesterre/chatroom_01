#include <iostream>
#include "user.h"

int main() {
    User user1;
    User user2(1, "Alice");

    std::cout << "user1 id: " << user1.getId() << std::endl;
    std::cout << "user1 name: " << user1.getName() << std::endl;

    std::cout << "user2 id: " << user2.getId() << std::endl;
    std::cout << "user2 name: " << user2.getName() << std::endl;

    user2.setName("Bob");
    std::cout << "user2 new name: " << user2.getName() << std::endl;

    return 0;
}