#include "server.h"

#include "http_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace {
constexpr unsigned short kPort = 8080;
constexpr int kBacklog = SOMAXCONN;
constexpr int kBufferSize = 8192;
}

HttpServer::HttpServer() : listenSocket_(INVALID_SOCKET), documentRoot_("public"), running_(false) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::setDocumentRoot(const std::string& root) {
    documentRoot_ = root;
}

bool HttpServer::initializeWinsock() {
    WSADATA wsaData{};
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed with error: " << result << '\n';
        return false;
    }
    return true;
}

bool HttpServer::createAndBindSocket() {
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "socket() failed with error: " << WSAGetLastError() << '\n';
        return false;
    }

    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(kPort);

    if (bind(listenSocket_, reinterpret_cast<SOCKADDR*>(&service), sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "bind() failed with error: " << WSAGetLastError() << '\n';
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen(listenSocket_, kBacklog) == SOCKET_ERROR) {
        std::cerr << "listen() failed with error: " << WSAGetLastError() << '\n';
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool HttpServer::start() {
    if (running_) {
        return true;
    }

    if (!initializeWinsock()) {
        return false;
    }

    if (!createAndBindSocket()) {
        WSACleanup();
        return false;
    }

    running_ = true;
    std::cout << "HTTP server started on port " << kPort << '\n';
    acceptLoop();
    return true;
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    WSACleanup();
    std::cout << "HTTP server stopped." << '\n';
}

void HttpServer::acceptLoop() {
    while (running_) {
        SOCKET clientSocket = accept(listenSocket_, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "accept() failed with error: " << WSAGetLastError() << '\n';
            }
            continue;
        }

        std::thread(&HttpServer::handleClient, this, clientSocket).detach();
    }
}

std::string HttpServer::sanitizePath(const std::string& requestPath) {
    std::string path = requestPath;

    const std::size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }

    if (path.empty() || path == "/") {
        return "/index.html";
    }

    if (path.find("..") != std::string::npos) {
        return "/index.html";
    }

    return path;
}

std::string HttpServer::getMimeType(const std::string& filePath) {
    std::string ext;
    const std::size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
    }

    if (ext == ".html" || ext == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (ext == ".css") {
        return "text/css; charset=utf-8";
    }
    if (ext == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (ext == ".txt") {
        return "text/plain; charset=utf-8";
    }

    return "application/octet-stream";
}

void HttpServer::handleClient(SOCKET clientSocket) {
    char buffer[kBufferSize] = {0};
    const int bytesReceived = recv(clientSocket, buffer, kBufferSize - 1, 0);

    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }

    const std::string rawRequest(buffer, static_cast<std::size_t>(bytesReceived));
    HttpRequest request;

    if (!parseHttpGetRequest(rawRequest, request)) {
        const std::string body = "400 Bad Request";
        const std::string response = buildHttpResponse(400, "Bad Request", "text/plain; charset=utf-8", body);
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(clientSocket);
        return;
    }

    const std::string safePath = sanitizePath(request.path);

    std::string fullPath = documentRoot_;
    if (!fullPath.empty() && fullPath.back() != '/' && fullPath.back() != '\\') {
        fullPath += '/';
    }
    fullPath += safePath.substr(1);

    std::ifstream file(fullPath, std::ios::binary);
    if (!file) {
        const std::string body = "404 Not Found";
        const std::string response = buildHttpResponse(404, "Not Found", "text/plain; charset=utf-8", body);
        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(clientSocket);
        return;
    }

    std::ostringstream contentStream;
    contentStream << file.rdbuf();
    const std::string body = contentStream.str();
    const std::string response = buildHttpResponse(200, "OK", getMimeType(fullPath), body);

    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    closesocket(clientSocket);
}
