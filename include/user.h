#ifndef USER_H
#define USER_H

#include <string>

class User{
private:
    int id;
    std::string name;

public:
    User();    //默认构造函数
    User(int id, std::string name);    //带参数的构造函数

    int getId() const;
    std::string getName() const;

    void setName(const std::string& name);
};

#endif 