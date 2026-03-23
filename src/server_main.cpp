/*
创建一个服务端对象
启动服务端
进入运行状态
*/

#include "tcp_server.h"


#include <iostream>

int main() {
    TcpServer server("127.0.0.1", 8888);

    if (!server.start()){
        std::cerr << "Failed to start the server." << std::endl;  //cerr是标准错误输出流，输出错误信息
        return 1;
    }

    server.run();
    return 0;
}