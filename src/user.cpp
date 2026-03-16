#include "user.h"

User::User(){
    id = 0;
    name = "unknown";
}

User::User(int id, const std::string&name){
    this->id = id;
    this->name = name;
}

int User::getId() const{
    return id;
}

std::string User::getName() const{
    return name;
}

void User::setName(const std::string& name){
    this->name = name;
}