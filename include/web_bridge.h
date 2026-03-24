#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class WebBridge {
public:
    WebBridge(const std::string& backend_ip, int backend_port, const std::string& host, int port);
    ~WebBridge();

    bool start();
    void run();

private:
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string body;
    };

    bool setupHttpListen();
    void stop();

    void acceptLoop();
    void handleHttpClient(int fd);

    bool connectBackend(const std::string& nickname, std::string& error);
    void disconnectBackend();
    bool sendBackendMessage(const std::string& message, std::string& error);

    void backendReadLoop();
    void pushSse(const std::string& event_name, const std::string& json_payload);

    static std::string readAllRequest(int fd);
    static bool parseHttpRequest(const std::string& raw, HttpRequest& req);
    static std::string jsonEscape(const std::string& text);
    static std::string getJsonString(const std::string& body, const std::string& key);

    static void writeHttpResponse(int fd,
                                  int status,
                                  const std::string& status_text,
                                  const std::string& content_type,
                                  const std::string& body);
    static void writeSseHeaders(int fd);

    static std::string loadFile(const std::string& path);

private:
    std::string backend_ip_;
    int backend_port_;

    std::string host_;
    int port_;

    int listen_fd_;
    int backend_fd_;

    std::atomic<bool> running_;
    std::atomic<bool> backend_connected_;

    std::string nickname_;

    std::mutex backend_mutex_;
    std::thread backend_reader_;

    std::mutex sse_mutex_;
    std::vector<int> sse_clients_;
};
