#include "user.h"

User::User(){
    id = 0;
    name = "unknown";
}

User::User(int id, const std::string& name){   //const：表示该参数在函数内部不会被修改，&避免复制，提高效率
    this->id = id;
    this->name = name;
}

int User::getId() const{  //函数后面的const：表示该函数不会修改对象的成员变量，只是读取，可以在常量对象上调用
    return id;
}

std::string User::getName() const{
    return name;
}

void User::setName(const std::string& name){
    this->name = name;  //this指针：指向当前对象的指针，使用this->name区分成员变量和参数name
}