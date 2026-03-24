#include "web_bridge.h"

#include <iostream>

int main() {
    WebBridge bridge("127.0.0.1", 8888, "127.0.0.1", 8080);
    if (!bridge.start()) {
        std::cerr << "Failed to start web bridge." << std::endl;
        return 1;
    }

    bridge.run();
    return 0;
}
