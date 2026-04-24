#include "server.h"

#include <csignal>
#include <iostream>

namespace {
HttpServer* g_server = nullptr;

void onSignal(int signalNumber) {
    if (signalNumber == SIGINT && g_server != nullptr) {
        std::cout << "\nCtrl+C received. Shutting down..." << '\n';
        g_server->stop();
    }
}
}

int main() {
    HttpServer server;
    server.setDocumentRoot("public");

    g_server = &server;
    std::signal(SIGINT, onSignal);

    if (!server.start()) {
        std::cerr << "Failed to start HTTP server." << '\n';
        return 1;
    }

    return 0;
}
