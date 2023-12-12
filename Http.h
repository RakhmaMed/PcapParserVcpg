#pragma once

#include "Utility.h"
#include "PatternSeeker.h"

#include <HttpLayer.h>

#include <iostream>
#include <optional>
#include <unordered_map>

using http_method_t = pcpp::HttpRequestLayer::HttpMethod;
using namespace PatterSeekerNS;

class HttpRequest
{
    std::shared_ptr<std::string> m_data{ new std::string };
    std::string_view m_method;
    std::string_view m_uri;
    std::string_view m_body;
    util::headers_view_t m_headers;

public:
    bool isEmpty() const {
        return m_data->empty();
    }
    void append(std::string_view newData) {
        m_data->append(newData);
    }

    std::string to_string() {
        return *m_data;
    }

    bool parse() {
        // method
        static const std::string_view GET = "GET";
        static const std::string_view POST = "POST";

        PatternSeeker parser{ *m_data };
        if (parser.expect(GET))
            m_method = GET;
        if (parser.expect(POST))
            m_method = POST;

        if (m_method.empty())
            return false;

        // uri
        if (!parser.startsWith(" /"))
            return false;

        auto uri = parser.extract(" ", " HTTP/1.1", move_after);
        if (uri.isEmpty())
            return false;

        m_uri = uri.to_string_view();

        // headers
        m_headers = util::parseHeaders(parser.extract("\r\n\r\n", move_after));
        if (m_headers.empty())
            return false;

        // body
        auto lengthOpt = PatternSeeker(m_headers["Content-Length"]).takeUInt64();
        if (!lengthOpt)
            return false;

        auto length = *lengthOpt;

        auto body = parser.extract(length);
        if (body.isEmpty() && length > 0)
            return false;

        if (body.size() != length) {
            std::cout << "WARNING! Body size is not equal to length\n";
        }
        m_body = body.to_string_view();

        return true;
    }
    std::string_view method() {
        return m_method;
    }

    std::string_view uri() {
        return m_uri;
    }

    const util::headers_view_t& headers() {
        return m_headers;
    }

    std::string_view body() {
        return m_body;
    }

    friend std::ostream& operator<<(std::ostream& oss, HttpRequest& req) {
        oss << req.method() << ' ' << req.uri();
        for (auto&& [header, val] : req.headers()) {
            oss << '\n' << header << ": " << val;
        }
        if (!req.body().empty())
            oss << "\n" << req.body() << '\n';
        return oss;
    }
};

struct HttpResponse
{
    std::shared_ptr<std::string> m_data{ new std::string };
    uint32_t m_code;
    util::headers_view_t m_headers;
    std::string_view m_body;
public:
    bool isEmpty() const {
        return m_data->empty();
    }
    void append(std::string_view newData) {
        m_data->append(newData);
    }
    std::string to_string() {
        return *m_data;
    }

    bool parse() {
        PatternSeeker parser{ *m_data };
        if (!parser.expect("HTTP/1.1 "))
            return false;

        auto code = parser.takeUInt64();
        if (!code)
            return false;
        m_code = static_cast<uint32_t>(*code);

        parser.extract("\n", move_after);

        m_headers = util::parseHeaders(parser.extract("\r\n\r\n", move_after));
        if (m_headers.empty())
            return false;

        // body
        auto lengthOpt = PatternSeeker(m_headers["Content-Length"]).takeUInt64();
        if (!lengthOpt)
            return false;

        auto length = *lengthOpt;

        auto body = parser.extract(length);
        if (body.isEmpty() && length > 0)
            return false;

        if (body.size() != length) {
            std::cout << "WARNING! Body size is not equal to length\n";
        }
        m_body = body.to_string_view();

        return true;
    }

    friend std::ostream& operator<<(std::ostream& oss, HttpResponse& resp) {
        oss << resp.m_code;
        for (auto&& [header, val] : resp.m_headers) {
            oss << '\n' << header << ": " << val;
        }
        if (!resp.m_body.empty())
            oss << "\n" << resp.m_body << '\n';
        return oss;
    }
};

struct RequestResponse
{
    HttpRequest request;
    HttpResponse response;
    
    bool isEmpty() const {
        return request.isEmpty() && response.isEmpty();
    }

    bool parse() {
        return request.parse() && response.parse();
    }

    std::string to_string() {
        return request.to_string() + "\n" + response.to_string();;
    }


    friend std::ostream& operator<<(std::ostream& oss, RequestResponse& r) {
        oss << r.request << "\n\n" << r.response << "\n------------------\n";
        return oss;
    }
};