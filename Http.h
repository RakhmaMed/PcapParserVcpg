#pragma once

#include "Utility.h"

#include <HttpLayer.h>

#include <optional>

using http_method_t = pcpp::HttpRequestLayer::HttpMethod;

struct HttpRequest
{
    timestamp_ms time;
    ConnInfo conninfo;
    http_method_t method;
    std::string uri;
    std::string body;

    friend std::ostream& operator<<(std::ostream& oss, const HttpRequest& req) {
        oss << req.conninfo << ' ' << req.time << ' ' << req.method << ' ' << req.uri;
        if (!req.body.empty())
            oss << "\n" << req.body << '\n';
        return oss;
    }
};

struct HttpResponse
{
    timestamp_ms time;
    ConnInfo conninfo;
    std::string body;

    friend std::ostream& operator<<(std::ostream& oss, const HttpResponse& resp) {
        oss << resp.conninfo << ' ' << resp.time << ' ' << resp.body;
        return oss;
    }
};

struct RequestResponse
{
    HttpRequest request;
    HttpResponse response;

    friend std::ostream& operator<<(std::ostream& oss, const RequestResponse& r) {
        oss << r.request << '\n\n' << r.response << "\n------------------\n";
        return oss;
    }
};