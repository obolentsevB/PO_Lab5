#pragma once

#include <atomic>
#include <string>

#include <winsock2.h>

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    void setDocumentRoot(const std::string& root);
    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(SOCKET clientSocket);

    static std::string sanitizePath(const std::string& requestPath);
    static std::string getMimeType(const std::string& filePath);

    bool initializeWinsock();
    bool createAndBindSocket();

    SOCKET listenSocket_;
    std::string documentRoot_;
    std::atomic<bool> running_;
};
