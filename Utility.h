#pragma once

#include <string>
#include <sstream>

//enum class timestamp_ms : uint64_t {};

using timestamp_ms = uint64_t;

timestamp_ms convertToTimestamp(const timespec& timeValue)
{
    return timeValue.tv_sec * 1000ULL + timeValue.tv_nsec / 1000000ULL;
}

struct ConnInfo {
    std::string source_ip;
    std::string dest_ip;
    uint16_t    source_port;
    uint16_t    dest_port;

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

