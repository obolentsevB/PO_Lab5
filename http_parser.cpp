#include "http_parser.h"

#include <sstream>

bool parseHttpGetRequest(const std::string& rawRequest, HttpRequest& request) {
    std::istringstream stream(rawRequest);

    std::string requestLine;
    if (!std::getline(stream, requestLine)) {
        return false;
    }

    // std::getline з '\n' зберігає можливий кінцевий '\r' від CRLF.
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream lineStream(requestLine);
    if (!(lineStream >> request.method >> request.path >> request.version)) {
        return false;
    }

    // Відхиляємо некоректні рядки запиту з додатковими токенами.
    std::string extra;
    if (lineStream >> extra) {
        return false;
    }

    return request.method == "GET" && request.version == "HTTP/1.1";
}

std::string buildHttpResponse(
    int statusCode,
    const std::string& reasonPhrase,
    const std::string& contentType,
    const std::string& body
) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << ' ' << reasonPhrase << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";

    // Обов'язковий порожній рядок між заголовками та тілом відповіді.
    response << "\r\n";

    response << body;
    return response.str();
}
