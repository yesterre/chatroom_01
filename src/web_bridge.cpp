#include "web_bridge.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <system_error>

namespace {
constexpr int kBufferSize = 4096;

void safeClose(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

bool sendAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string executableDir() {
    std::array<char, PATH_MAX> buf{};
    const ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n <= 0) {
        return "";
    }

    buf[static_cast<size_t>(n)] = 0;
    std::filesystem::path exe_path(buf.data());
    return exe_path.parent_path().string();
}

std::string resolveAssetPath(const std::string& relative_path) {
    const std::string exe_dir = executableDir();

    std::vector<std::string> candidates = {
        relative_path,
        std::string("../") + relative_path,
        std::string("../../") + relative_path
    };

    if (exe_dir.empty() == false) {
        candidates.push_back(exe_dir + "/" + relative_path);
        candidates.push_back(exe_dir + "/../" + relative_path);
        candidates.push_back(exe_dir + "/../../" + relative_path);
    }

    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && ec.value() == 0) {
            return path;
        }
    }

    return relative_path;
}

std::string makeEventPacket(const std::string& event_name, const std::string& json_payload) {
    return std::string("event: ") + event_name + "\n" + "data: " + json_payload + "\n\n";
}
}  // namespace

WebBridge::WebBridge(const std::string& backend_ip, int backend_port, const std::string& host, int port)
    : backend_ip_(backend_ip),
      backend_port_(backend_port),
      host_(host),
      port_(port),
      listen_fd_(-1),
      running_(false) {}

WebBridge::~WebBridge() {
    stop();
}

bool WebBridge::start() {
    if (setupHttpListen() == false) {
        return false;
    }

    running_ = true;
    std::cout << "Web bridge started at http://" << host_ << ":" << port_ << std::endl;
    std::cout << "Bridge backend target: " << backend_ip_ << ":" << backend_port_ << std::endl;
    return true;
}

void WebBridge::run() {
    acceptLoop();
}

bool WebBridge::setupHttpListen() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "web bridge socket() failed" << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "web bridge setsockopt() failed" << std::endl;
        safeClose(listen_fd_);
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "web bridge inet_pton() failed for host: " << host_ << std::endl;
        safeClose(listen_fd_);
        return false;
    }

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "web bridge bind() failed" << std::endl;
        safeClose(listen_fd_);
        return false;
    }

    if (listen(listen_fd_, 64) < 0) {
        std::cerr << "web bridge listen() failed" << std::endl;
        safeClose(listen_fd_);
        return false;
    }

    return true;
}

void WebBridge::stop() {
    running_ = false;
    safeClose(listen_fd_);

    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_copy.swap(sessions_);
    }

    for (auto& pair : sessions_copy) {
        const std::shared_ptr<Session>& session = pair.second;
        std::thread reader;

        {
            std::lock_guard<std::mutex> lock(session->mutex);
            if (session->backend_fd >= 0) {
                shutdown(session->backend_fd, SHUT_RDWR);
                close(session->backend_fd);
                session->backend_fd = -1;
            }
            session->connected = false;
            session->nickname.clear();

            for (int sse_fd : session->sse_clients) {
                if (sse_fd >= 0) {
                    close(sse_fd);
                }
            }
            session->sse_clients.clear();

            if (session->backend_reader.joinable()) {
                reader = std::move(session->backend_reader);
            }
        }

        if (reader.joinable()) {
            reader.join();
        }
    }
}

void WebBridge::acceptLoop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (running_ == false) {
                break;
            }
            std::cerr << "web bridge accept() failed" << std::endl;
            continue;
        }

        std::thread(&WebBridge::handleHttpClient, this, client_fd).detach();
    }
}

std::shared_ptr<WebBridge::Session> WebBridge::getOrCreateSession(const std::string& sid) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(sid);
    if (it != sessions_.end()) {
        return it->second;
    }

    auto session = std::make_shared<Session>();
    sessions_[sid] = session;
    return session;
}

void WebBridge::handleHttpClient(int fd) {
    std::string raw = readAllRequest(fd);
    HttpRequest req;
    if (parseHttpRequest(raw, req) == false) {
        writeHttpResponse(fd, 400, "Bad Request", "application/json",
                          "{\"ok\":false,\"error\":\"bad request\"}");
        close(fd);
        return;
    }

    const std::string path = routePath(req.path);

    if (req.method == "GET" && path == "/") {
        std::string html = loadFile("web/index.html");
        if (html.empty()) {
            writeHttpResponse(fd, 500, "Internal Server Error", "text/plain", "Missing web/index.html");
        } else {
            writeHttpResponse(fd, 200, "OK", "text/html; charset=utf-8", html);
        }
        close(fd);
        return;
    }

    if (req.method == "GET" && path == "/style.css") {
        std::string css = loadFile("web/style.css");
        writeHttpResponse(fd, css.empty() ? 404 : 200, css.empty() ? "Not Found" : "OK",
                          "text/css; charset=utf-8", css.empty() ? "" : css);
        close(fd);
        return;
    }

    if (req.method == "GET" && path == "/app.js") {
        std::string js = loadFile("web/app.js");
        writeHttpResponse(fd, js.empty() ? 404 : 200, js.empty() ? "Not Found" : "OK",
                          "application/javascript; charset=utf-8", js.empty() ? "" : js);
        close(fd);
        return;
    }

    if (req.method == "GET" && path == "/api/events") {
        const std::string sid = queryParam(req.path, "sid");
        if (validSid(sid) == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"invalid sid\"}");
            close(fd);
            return;
        }

        auto session = getOrCreateSession(sid);

        writeSseHeaders(fd);
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            session->sse_clients.push_back(fd);

            std::string payload = std::string("{\"connected\":") + (session->connected ? "true" : "false") +
                                  ",\"nickname\":\"" + jsonEscape(session->nickname) + "\"}";
            sendAll(fd, makeEventPacket("status", payload));
        }

        const std::vector<std::string> users = snapshotOnlineUsers();
        sendAll(fd, makeEventPacket("users", usersJsonPayload(users)));

        char keep_alive;
        while (recv(fd, &keep_alive, 1, MSG_DONTWAIT) != 0) {
            usleep(200000);
        }

        {
            std::lock_guard<std::mutex> lock(session->mutex);
            auto it = session->sse_clients.begin();
            while (it != session->sse_clients.end()) {
                if (*it == fd) {
                    it = session->sse_clients.erase(it);
                } else {
                    ++it;
                }
            }
        }

        close(fd);
        return;
    }

    if (req.method == "POST" && path == "/api/connect") {
        const std::string sid = getJsonString(req.body, "sid");
        const std::string nickname = getJsonString(req.body, "nickname");

        if (validSid(sid) == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"invalid sid\"}");
            close(fd);
            return;
        }

        if (nickname.empty()) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"nickname is required\"}");
            close(fd);
            return;
        }

        std::string error;
        bool ok = connectBackend(sid, nickname, error);
        if (ok == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}");
            close(fd);
            return;
        }

        writeHttpResponse(fd, 200, "OK", "application/json",
                          "{\"ok\":true,\"nickname\":\"" + jsonEscape(nickname) + "\"}");
        close(fd);
        return;
    }

    if (req.method == "POST" && path == "/api/send") {
        const std::string sid = getJsonString(req.body, "sid");
        const std::string message = getJsonString(req.body, "message");

        if (validSid(sid) == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"invalid sid\"}");
            close(fd);
            return;
        }

        if (message.empty()) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"message is required\"}");
            close(fd);
            return;
        }

        std::string error;
        bool ok = sendBackendMessage(sid, message, error);
        if (ok == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}");
            close(fd);
            return;
        }

        writeHttpResponse(fd, 200, "OK", "application/json", "{\"ok\":true}");
        close(fd);
        return;
    }

    if (req.method == "POST" && path == "/api/disconnect") {
        const std::string sid = getJsonString(req.body, "sid");
        if (validSid(sid) == false) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"invalid sid\"}");
            close(fd);
            return;
        }

        disconnectBackend(sid);
        writeHttpResponse(fd, 200, "OK", "application/json", "{\"ok\":true}");
        close(fd);
        return;
    }

    writeHttpResponse(fd, 404, "Not Found", "application/json",
                      "{\"ok\":false,\"error\":\"route not found\"}");
    close(fd);
}

bool WebBridge::connectBackend(const std::string& sid, const std::string& nickname, std::string& error) {
    auto session = getOrCreateSession(sid);

    {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (session->connected) {
            error = "already connected";
            return false;
        }
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = "socket() failed";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(backend_port_);
    if (inet_pton(AF_INET, backend_ip_.c_str(), &addr.sin_addr) <= 0) {
        close(fd);
        error = "inet_pton() failed";
        return false;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        error = "connect() to tcp server failed";
        return false;
    }

    if (sendAll(fd, nickname) == false) {
        close(fd);
        error = "failed to send nickname";
        return false;
    }

    std::thread old_reader;
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (session->backend_reader.joinable()) {
            old_reader = std::move(session->backend_reader);
        }

        session->backend_fd = fd;
        session->connected = true;
        session->nickname = nickname;
        session->backend_reader = std::thread(&WebBridge::backendReadLoop, this, sid, session);
    }

    if (old_reader.joinable()) {
        old_reader.join();
    }

    pushSessionStatus(session);
    pushSse(session, "system", "{\"text\":\"connected to backend\"}");
    broadcastUsersSnapshot();
    return true;
}

void WebBridge::disconnectBackend(const std::string& sid) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(sid);
        if (it == sessions_.end()) {
            return;
        }
        session = it->second;
    }

    bool was_connected = false;
    std::thread reader;
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        was_connected = session->connected;

        if (session->backend_fd >= 0) {
            shutdown(session->backend_fd, SHUT_RDWR);
            close(session->backend_fd);
            session->backend_fd = -1;
        }

        session->connected = false;
        session->nickname.clear();

        if (session->backend_reader.joinable()) {
            reader = std::move(session->backend_reader);
        }
    }

    if (reader.joinable() && reader.get_id() != std::this_thread::get_id()) {
        reader.join();
    }

    if (was_connected) {
        pushSessionStatus(session);
        pushSse(session, "system", "{\"text\":\"disconnected\"}");
        broadcastUsersSnapshot();
    }
}

bool WebBridge::sendBackendMessage(const std::string& sid, const std::string& message, std::string& error) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(sid);
        if (it == sessions_.end()) {
            error = "session not found";
            return false;
        }
        session = it->second;
    }

    std::lock_guard<std::mutex> lock(session->mutex);
    if (session->connected == false || session->backend_fd < 0) {
        error = "not connected to backend";
        return false;
    }

    if (sendAll(session->backend_fd, message) == false) {
        safeClose(session->backend_fd);
        session->connected = false;
        session->nickname.clear();
        error = "send to backend failed";
        return false;
    }

    return true;
}

void WebBridge::backendReadLoop(const std::string& sid, const std::shared_ptr<Session>& session) {
    (void)sid;
    char buffer[kBufferSize];

    while (true) {
        int local_fd = -1;
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            if (session->connected == false || session->backend_fd < 0) {
                break;
            }
            local_fd = session->backend_fd;
        }

        ssize_t n = recv(local_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            break;
        }

        buffer[n] = 0;
        std::string text(buffer);
        while (text.empty() == false && (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
        }

        if (text.empty()) {
            continue;
        }

        if (startsWith(text, "[System]")) {
            pushSse(session, "system", "{\"text\":\"" + jsonEscape(text) + "\"}");
        } else {
            pushSse(session, "message", "{\"text\":\"" + jsonEscape(text) + "\"}");
        }
    }

    bool was_connected = false;
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        was_connected = session->connected;
        safeClose(session->backend_fd);
        session->connected = false;
        session->nickname.clear();
    }

    if (was_connected) {
        pushSessionStatus(session);
        pushSse(session, "system", "{\"text\":\"backend disconnected\"}");
        broadcastUsersSnapshot();
    }
}

void WebBridge::pushSse(const std::shared_ptr<Session>& session,
                        const std::string& event_name,
                        const std::string& json_payload) {
    const std::string packet = makeEventPacket(event_name, json_payload);

    std::lock_guard<std::mutex> lock(session->mutex);
    auto it = session->sse_clients.begin();
    while (it != session->sse_clients.end()) {
        if (*it < 0 || sendAll(*it, packet) == false) {
            if (*it >= 0) {
                close(*it);
            }
            it = session->sse_clients.erase(it);
        } else {
            ++it;
        }
    }
}

void WebBridge::pushSessionStatus(const std::shared_ptr<Session>& session) {
    bool connected = false;
    std::string nickname;

    {
        std::lock_guard<std::mutex> lock(session->mutex);
        connected = session->connected;
        nickname = session->nickname;
    }

    const std::string payload = std::string("{\"connected\":") + (connected ? "true" : "false") +
                                ",\"nickname\":\"" + jsonEscape(nickname) + "\"}";
    pushSse(session, "status", payload);
}

std::vector<std::string> WebBridge::snapshotOnlineUsers() {
    std::vector<std::string> users;

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& pair : sessions_) {
        const auto& session = pair.second;
        std::lock_guard<std::mutex> session_lock(session->mutex);
        if (session->connected && session->nickname.empty() == false) {
            users.push_back(session->nickname);
        }
    }

    std::sort(users.begin(), users.end());
    users.erase(std::unique(users.begin(), users.end()), users.end());
    return users;
}

void WebBridge::broadcastUsersSnapshot() {
    const std::vector<std::string> users = snapshotOnlineUsers();
    const std::string payload = usersJsonPayload(users);

    std::vector<std::shared_ptr<Session>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_copy.reserve(sessions_.size());
        for (const auto& pair : sessions_) {
            sessions_copy.push_back(pair.second);
        }
    }

    for (const auto& session : sessions_copy) {
        pushSse(session, "users", payload);
    }
}

std::string WebBridge::readAllRequest(int fd) {
    std::string raw;
    char buffer[kBufferSize];

    while (true) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        raw.append(buffer, buffer + n);

        size_t header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            size_t content_length = 0;
            std::string headers = raw.substr(0, header_end);
            size_t pos = headers.find("Content-Length:");
            if (pos != std::string::npos) {
                size_t value_start = headers.find_first_of("0123456789", pos);
                if (value_start != std::string::npos) {
                    content_length = static_cast<size_t>(std::stoi(headers.substr(value_start)));
                }
            }

            size_t body_have = raw.size() - (header_end + 4);
            if (body_have >= content_length) {
                break;
            }
        }

        if (raw.size() > 1024 * 1024) {
            break;
        }
    }

    return raw;
}

bool WebBridge::parseHttpRequest(const std::string& raw, HttpRequest& req) {
    const size_t line_end = raw.find("\r\n");
    if (line_end == std::string::npos) {
        return false;
    }

    std::string request_line = raw.substr(0, line_end);
    std::istringstream iss(request_line);
    std::string version;
    if (!(iss >> req.method >> req.path >> version)) {
        return false;
    }

    const size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        req.body.clear();
        return true;
    }

    req.body = raw.substr(header_end + 4);
    return true;
}

std::string WebBridge::jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 16);

    for (char c : text) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }

    return out;
}

std::string WebBridge::getJsonString(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const size_t key_pos = body.find(marker);
    if (key_pos == std::string::npos) {
        return "";
    }

    const size_t colon = body.find(':', key_pos + marker.size());
    if (colon == std::string::npos) {
        return "";
    }

    const size_t quote_start = body.find('"', colon + 1);
    if (quote_start == std::string::npos) {
        return "";
    }

    std::string value;
    for (size_t i = quote_start + 1; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '\\' && i + 1 < body.size()) {
            const char next = body[i + 1];
            switch (next) {
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case '"':
                    value.push_back('"');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                default:
                    value.push_back(next);
                    break;
            }
            ++i;
            continue;
        }

        if (c == '"') {
            return value;
        }
        value.push_back(c);
    }

    return "";
}

std::string WebBridge::routePath(const std::string& path) {
    const size_t q = path.find('?');
    if (q == std::string::npos) {
        return path;
    }
    return path.substr(0, q);
}

std::string WebBridge::queryParam(const std::string& path, const std::string& key) {
    const size_t q = path.find('?');
    if (q == std::string::npos || q + 1 >= path.size()) {
        return "";
    }

    const std::string query = path.substr(q + 1);
    const std::string target = key + "=";

    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }

        const std::string part = query.substr(start, end - start);
        if (part.rfind(target, 0) == 0) {
            return part.substr(target.size());
        }

        start = end + 1;
    }

    return "";
}

bool WebBridge::validSid(const std::string& sid) {
    if (sid.empty() || sid.size() > 64) {
        return false;
    }

    for (char c : sid) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            continue;
        }
        if (c == '-' || c == '_') {
            continue;
        }
        return false;
    }

    return true;
}

bool WebBridge::startsWith(const std::string& text, const std::string& prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string WebBridge::usersJsonPayload(const std::vector<std::string>& users) {
    std::string json = "{\"users\":[";

    for (size_t i = 0; i < users.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "\"" + jsonEscape(users[i]) + "\"";
    }

    json += "],\"count\":" + std::to_string(users.size()) + "}";
    return json;
}

void WebBridge::writeHttpResponse(int fd,
                                  int status,
                                  const std::string& status_text,
                                  const std::string& content_type,
                                  const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << status_text << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << body;

    sendAll(fd, oss.str());
}

void WebBridge::writeSseHeaders(int fd) {
    const std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";
    sendAll(fd, headers);
}

std::string WebBridge::loadFile(const std::string& path) {
    const std::string resolved_path = resolveAssetPath(path);

    std::ifstream in(resolved_path, std::ios::binary);
    if (in.is_open() == false) {
        return "";
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
