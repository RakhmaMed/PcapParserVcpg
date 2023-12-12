#pragma once

#include "PatternSeeker.h"

#include <TcpReassembly.h>

#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#define UNUSED(x) (void)(x);


namespace util
{

//enum class timestamp_ms : uint64_t {};

using timestamp_ms = uint64_t;

timestamp_ms convertToTimestamp(const timeval& timeValue)
{
    return timeValue.tv_sec * 1000ULL + timeValue.tv_usec / 1000ULL;
}

std::string_view trim(std::string_view in)
{
    auto left = in.begin();
    while (true) {
        if (left == in.end())
            return {};
        if (!isspace(*left))
            break;
        left += 1;
    }
    auto right = in.end() - 1;
    while (right > left && isspace(*right))
    {
        right -= 1;
    }

    return in.substr(std::distance(in.begin(), left), std::distance(left, right) + 1);
}

std::string trim(std::string in)
{
    auto left = in.begin();
    while (true) {
        if (left == in.end())
            return {};
        if (!isspace(*left))
            break;
        left += 1;
    }
    auto right = in.end() - 1;
    while (right > left && isspace(*right))
    {
        right -= 1;
    }

    return in.substr(std::distance(in.begin(), left), std::distance(left, right) + 1);
}


std::string replace(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}


bool isHttpPort(const pcpp::ConnectionData& connData) {
    return connData.dstPort == 80;
}

bool isRtspPort(const pcpp::ConnectionData& connData) {
    return connData.dstPort == 554 || connData.srcPort == 554;
}

struct ConnInfo {
    std::string source_ip;
    std::string dest_ip;
    uint16_t    source_port;
    uint16_t    dest_port;

    bool isEmpty() {
        return source_ip.empty() && dest_ip.empty() && !source_port && !dest_port;
    }
    
    friend std::ostream& operator<<(std::ostream& oss, const ConnInfo& info) {
        oss << info.source_ip << ':' << info.source_port << " -> " << info.dest_ip << ':' << info.dest_port;
        return oss;
    }

    auto tie() const {
        return std::tie(source_ip, dest_ip, source_port, dest_port);
    }

    bool operator==(const ConnInfo& other) const {
        return tie() == other.tie();
    }
};

struct ConnInfoHash {
    size_t operator()(const ConnInfo& info) const {
        return std::hash<std::string>()(info.source_ip) ^
            std::hash<std::string>()(info.dest_ip) ^
            std::hash<uint16_t>()(info.source_port) ^
            std::hash<uint16_t>()(info.dest_port);
    }
};

using headers_view_t = std::unordered_map<std::string_view, std::string_view>;

headers_view_t parseHeaders(PatterSeekerNS::PatternSeeker parser) {
    headers_view_t headers{};
    while (parser.isNotEmpty()) {
        parser.skipWhiteSpaces();
        auto name = parser.extract(":", PatterSeekerNS::move_after);
        if (name.isEmpty())
            break;
        auto val = parser.extract("\n", PatterSeekerNS::move_after);
        if (val.isEmpty())
            val = parser;
        headers.emplace(name.to_string_view(), trim(val.to_string_view()));
    }
    return headers;
}
}