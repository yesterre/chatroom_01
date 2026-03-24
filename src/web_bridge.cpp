#include "web_bridge.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <array>
#include <limits.h>

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
    ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
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

    if (!exe_dir.empty()) {
        candidates.push_back(exe_dir + "/" + relative_path);
        candidates.push_back(exe_dir + "/../" + relative_path);
        candidates.push_back(exe_dir + "/../../" + relative_path);
    }

    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            return path;
        }
    }

    return relative_path;
}
}  // namespace

WebBridge::WebBridge(const std::string& backend_ip, int backend_port, const std::string& host, int port)
    : backend_ip_(backend_ip),
      backend_port_(backend_port),
      host_(host),
      port_(port),
      listen_fd_(-1),
      backend_fd_(-1),
      running_(false),
      backend_connected_(false) {}

WebBridge::~WebBridge() {
    stop();
}

bool WebBridge::start() {
    if (!setupHttpListen()) {
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

    if (listen(listen_fd_, 16) < 0) {
        std::cerr << "web bridge listen() failed" << std::endl;
        safeClose(listen_fd_);
        return false;
    }

    return true;
}

void WebBridge::stop() {
    running_ = false;

    disconnectBackend();
    safeClose(listen_fd_);

    std::lock_guard<std::mutex> lock(sse_mutex_);
    for (int fd : sse_clients_) {
        if (fd >= 0) {
            close(fd);
        }
    }
    sse_clients_.clear();
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
            std::cerr << "web bridge accept() failed" << std::endl;
            continue;
        }
        std::thread(&WebBridge::handleHttpClient, this, client_fd).detach();
    }
}

void WebBridge::handleHttpClient(int fd) {
    std::string raw = readAllRequest(fd);
    HttpRequest req;
    if (!parseHttpRequest(raw, req)) {
        writeHttpResponse(fd, 400, "Bad Request", "application/json",
                          "{\"ok\":false,\"error\":\"bad request\"}");
        close(fd);
        return;
    }

    if (req.method == "GET" && req.path == "/") {
        std::string html = loadFile("web/index.html");
        if (html.empty()) {
            writeHttpResponse(fd, 500, "Internal Server Error", "text/plain",
                              "Missing web/index.html");
            close(fd);
            return;
        }
        writeHttpResponse(fd, 200, "OK", "text/html; charset=utf-8", html);
        close(fd);
        return;
    }

    if (req.method == "GET" && req.path == "/style.css") {
        std::string css = loadFile("web/style.css");
        writeHttpResponse(fd, css.empty() ? 404 : 200, css.empty() ? "Not Found" : "OK",
                          "text/css; charset=utf-8", css.empty() ? "" : css);
        close(fd);
        return;
    }

    if (req.method == "GET" && req.path == "/app.js") {
        std::string js = loadFile("web/app.js");
        writeHttpResponse(fd, js.empty() ? 404 : 200, js.empty() ? "Not Found" : "OK",
                          "application/javascript; charset=utf-8", js.empty() ? "" : js);
        close(fd);
        return;
    }

    if (req.method == "GET" && req.path == "/api/events") {
        writeSseHeaders(fd);
        {
            std::lock_guard<std::mutex> lock(sse_mutex_);
            sse_clients_.push_back(fd);
        }

        std::string initial = std::string("event: status\n") +
                              "data: {\"connected\":" + (backend_connected_ ? "true" : "false") +
                              ",\"nickname\":\"" + jsonEscape(nickname_) + "\"}\n\n";
        sendAll(fd, initial);

        char keep_alive;
        while (recv(fd, &keep_alive, 1, MSG_DONTWAIT) != 0) {
            usleep(200000);
        }

        std::lock_guard<std::mutex> lock(sse_mutex_);
        auto it = sse_clients_.begin();
        while (it != sse_clients_.end()) {
            if (*it == fd) {
                it = sse_clients_.erase(it);
            } else {
                ++it;
            }
        }

        close(fd);
        return;
    }

    if (req.method == "POST" && req.path == "/api/connect") {
        std::string nickname = getJsonString(req.body, "nickname");
        if (nickname.empty()) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"nickname is required\"}");
            close(fd);
            return;
        }

        std::string error;
        bool ok = connectBackend(nickname, error);
        if (!ok) {
            writeHttpResponse(fd, 500, "Internal Server Error", "application/json",
                              "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}");
            close(fd);
            return;
        }

        writeHttpResponse(fd, 200, "OK", "application/json",
                          "{\"ok\":true,\"nickname\":\"" + jsonEscape(nickname_) + "\"}");
        close(fd);
        return;
    }

    if (req.method == "POST" && req.path == "/api/send") {
        std::string message = getJsonString(req.body, "message");
        if (message.empty()) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"message is required\"}");
            close(fd);
            return;
        }

        std::string error;
        bool ok = sendBackendMessage(message, error);
        if (!ok) {
            writeHttpResponse(fd, 400, "Bad Request", "application/json",
                              "{\"ok\":false,\"error\":\"" + jsonEscape(error) + "\"}");
            close(fd);
            return;
        }

        writeHttpResponse(fd, 200, "OK", "application/json", "{\"ok\":true}");
        close(fd);
        return;
    }

    if (req.method == "POST" && req.path == "/api/disconnect") {
        disconnectBackend();
        pushSse("status", "{\"connected\":false,\"nickname\":\"\"}");
        writeHttpResponse(fd, 200, "OK", "application/json", "{\"ok\":true}");
        close(fd);
        return;
    }

    writeHttpResponse(fd, 404, "Not Found", "application/json",
                      "{\"ok\":false,\"error\":\"route not found\"}");
    close(fd);
}

bool WebBridge::connectBackend(const std::string& nickname, std::string& error) {
    std::lock_guard<std::mutex> lock(backend_mutex_);

    if (backend_connected_) {
        error = "already connected";
        return false;
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

    if (!sendAll(fd, nickname)) {
        close(fd);
        error = "failed to send nickname";
        return false;
    }

    backend_fd_ = fd;
    nickname_ = nickname;
    backend_connected_ = true;

    if (backend_reader_.joinable()) {
        backend_reader_.join();
    }
    backend_reader_ = std::thread(&WebBridge::backendReadLoop, this);

    pushSse("status", "{\"connected\":true,\"nickname\":\"" + jsonEscape(nickname_) + "\"}");
    return true;
}

void WebBridge::disconnectBackend() {
    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        if (backend_fd_ >= 0) {
            shutdown(backend_fd_, SHUT_RDWR);
            close(backend_fd_);
            backend_fd_ = -1;
        }
        backend_connected_ = false;
        nickname_.clear();
    }

    if (backend_reader_.joinable()) {
        backend_reader_.join();
    }
}

bool WebBridge::sendBackendMessage(const std::string& message, std::string& error) {
    std::lock_guard<std::mutex> lock(backend_mutex_);

    if (!backend_connected_ || backend_fd_ < 0) {
        error = "not connected to backend";
        return false;
    }

    if (!sendAll(backend_fd_, message)) {
        backend_connected_ = false;
        safeClose(backend_fd_);
        error = "send to backend failed";
        return false;
    }

    return true;
}

void WebBridge::backendReadLoop() {
    char buffer[kBufferSize];

    while (backend_connected_) {
        int local_fd = -1;
        {
            std::lock_guard<std::mutex> lock(backend_mutex_);
            local_fd = backend_fd_;
        }

        if (local_fd < 0) {
            break;
        }

        ssize_t n = recv(local_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            break;
        }

        buffer[n] = '\0';
        std::string text(buffer);

        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
        }

        if (!text.empty()) {
            pushSse("message", "{\"text\":\"" + jsonEscape(text) + "\"}");
        }
    }

    {
        std::lock_guard<std::mutex> lock(backend_mutex_);
        backend_connected_ = false;
        safeClose(backend_fd_);
        nickname_.clear();
    }

    pushSse("status", "{\"connected\":false,\"nickname\":\"\"}");
    pushSse("system", "{\"text\":\"backend disconnected\"}");
}

void WebBridge::pushSse(const std::string& event_name, const std::string& json_payload) {
    std::string packet = "event: " + event_name + "\n" + "data: " + json_payload + "\n\n";

    std::lock_guard<std::mutex> lock(sse_mutex_);
    auto it = sse_clients_.begin();
    while (it != sse_clients_.end()) {
        if (*it < 0 || !sendAll(*it, packet)) {
            if (*it >= 0) {
                close(*it);
            }
            it = sse_clients_.erase(it);
        } else {
            ++it;
        }
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
    size_t line_end = raw.find("\r\n");
    if (line_end == std::string::npos) {
        return false;
    }

    std::string request_line = raw.substr(0, line_end);
    std::istringstream iss(request_line);
    std::string version;
    if (!(iss >> req.method >> req.path >> version)) {
        return false;
    }

    size_t header_end = raw.find("\r\n\r\n");
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
    std::string marker = "\"" + key + "\"";
    size_t key_pos = body.find(marker);
    if (key_pos == std::string::npos) {
        return "";
    }

    size_t colon = body.find(':', key_pos + marker.size());
    if (colon == std::string::npos) {
        return "";
    }

    size_t quote_start = body.find('"', colon + 1);
    if (quote_start == std::string::npos) {
        return "";
    }

    std::string value;
    for (size_t i = quote_start + 1; i < body.size(); ++i) {
        char c = body[i];
        if (c == '\\' && i + 1 < body.size()) {
            char next = body[i + 1];
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

    std::string resp = oss.str();
    sendAll(fd, resp);
}

void WebBridge::writeSseHeaders(int fd) {
    std::string headers =
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
    if (!in) {
        return "";
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
