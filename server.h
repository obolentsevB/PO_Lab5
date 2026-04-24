#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <winsock2.h>

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    void setDocumentRoot(const std::string& root);
    bool start();
    void stop();

private:
    void eventLoop(); // Замість acceptLoop
    void handleSocket(SOCKET s, fd_set& readSet);
    
    static std::string sanitizePath(const std::string& requestPath);
    static std::string getMimeType(const std::string& filePath);

    bool initializeWinsock();
    bool createAndBindSocket();
    void setNonBlocking(SOCKET s);

    SOCKET listenSocket_;
    std::string documentRoot_;
    std::atomic<bool> running_;
    std::vector<SOCKET> clientSockets_; // Список активних клієнтів
};
