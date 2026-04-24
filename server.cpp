#include "server.h"
#include "http_parser.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace {
constexpr unsigned short kPort = 8080;
constexpr int kBufferSize = 8192;
}

HttpServer::HttpServer() : listenSocket_(INVALID_SOCKET), documentRoot_("public"), running_(false) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::setDocumentRoot(const std::string& root) { documentRoot_ = root; }

bool HttpServer::initializeWinsock() {
    WSADATA wsaData{};
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

void HttpServer::setNonBlocking(SOCKET s) {
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
}

bool HttpServer::createAndBindSocket() {
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) return false;

    setNonBlocking(listenSocket_);

    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(kPort);

    if (bind(listenSocket_, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) return false;
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) return false;

    return true;
}

bool HttpServer::start() {
    if (!initializeWinsock() || !createAndBindSocket()) return false;
    running_ = true;
    std::cout << "Non-blocking HTTP server started on port " << kPort << std::endl;
    eventLoop();
    return true;
}

void HttpServer::eventLoop() {
    while (running_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket_, &readSet);

        for (SOCKET s : clientSockets_) {
            FD_SET(s, &readSet);
        }

        timeval timeout{0, 100000}; // 100ms timeout
        int activity = select(0, &readSet, nullptr, nullptr, &timeout);

        if (activity == SOCKET_ERROR) break;

        // Нове підключення
        if (FD_ISSET(listenSocket_, &readSet)) {
            SOCKET clientSocket = accept(listenSocket_, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                setNonBlocking(clientSocket);
                clientSockets_.push_back(clientSocket);
            }
        }

        // Обробка існуючих клієнтів
        for (auto it = clientSockets_.begin(); it != clientSockets_.end(); ) {
            if (FD_ISSET(*it, &readSet)) {
                handleSocket(*it, readSet);
                closesocket(*it);
                it = clientSockets_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void HttpServer::handleSocket(SOCKET clientSocket, fd_set& readSet) {
    char buffer[kBufferSize] = {0};
    int bytesReceived = recv(clientSocket, buffer, kBufferSize - 1, 0);

    if (bytesReceived > 0) {
        std::string rawRequest(buffer, bytesReceived);
        HttpRequest request;
        
        std::string response;
        if (parseHttpGetRequest(rawRequest, request)) {
            std::string safePath = sanitizePath(request.path);
            std::string fullPath = documentRoot_ + safePath;
            
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                std::ostringstream ss;
                ss << file.rdbuf();
                response = buildHttpResponse(200, "OK", getMimeType(fullPath), ss.str());
            } else {
                response = buildHttpResponse(404, "Not Found", "text/plain", "404 Not Found");
            }
        } else {
            response = buildHttpResponse(400, "Bad Request", "text/plain", "400 Bad Request");
        }
        send(clientSocket, response.c_str(), (int)response.size(), 0);
    }
}

std::string HttpServer::sanitizePath(const std::string& requestPath) {
    std::string path = requestPath;
    if (path == "/") return "/index.html";
    if (path.find("..") != std::string::npos) return "/index.html";
    return path;
}

std::string HttpServer::getMimeType(const std::string& filePath) {
    if (filePath.find(".html") != std::string::npos) return "text/html";
    return "application/octet-stream";
}

void HttpServer::stop() {
    running_ = false;
    closesocket(listenSocket_);
    for (SOCKET s : clientSockets_) closesocket(s);
    WSACleanup();
}
