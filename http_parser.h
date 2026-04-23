#pragma once

#include <string>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
};

bool parseHttpGetRequest(const std::string& rawRequest, HttpRequest& request);

std::string buildHttpResponse(
    int statusCode,
    const std::string& reasonPhrase,
    const std::string& contentType,
    const std::string& body
);
