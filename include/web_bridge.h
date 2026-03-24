#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
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

    struct Session {
        std::mutex mutex;
        int backend_fd = -1;
        bool connected = false;
        std::string nickname;
        std::thread backend_reader;
        std::vector<int> sse_clients;
    };

    bool setupHttpListen();
    void stop();

    void acceptLoop();
    void handleHttpClient(int fd);

    std::shared_ptr<Session> getOrCreateSession(const std::string& sid);

    bool connectBackend(const std::string& sid, const std::string& nickname, std::string& error);
    void disconnectBackend(const std::string& sid);
    bool sendBackendMessage(const std::string& sid, const std::string& message, std::string& error);

    void backendReadLoop(const std::string& sid, const std::shared_ptr<Session>& session);

    void pushSse(const std::shared_ptr<Session>& session,
                 const std::string& event_name,
                 const std::string& json_payload);
    void pushSessionStatus(const std::shared_ptr<Session>& session);
    void broadcastUsersSnapshot();
    std::vector<std::string> snapshotOnlineUsers();

    static std::string readAllRequest(int fd);
    static bool parseHttpRequest(const std::string& raw, HttpRequest& req);
    static std::string jsonEscape(const std::string& text);
    static std::string getJsonString(const std::string& body, const std::string& key);
    static std::string routePath(const std::string& path);
    static std::string queryParam(const std::string& path, const std::string& key);
    static bool validSid(const std::string& sid);
    static bool startsWith(const std::string& text, const std::string& prefix);
    static std::string usersJsonPayload(const std::vector<std::string>& users);

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
    std::atomic<bool> running_;

    std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};
