#pragma once

#include "PatternSeeker.h"

#include <TcpReassembly.h>

#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <span>
#include <cassert>

#include <boost/endian/arithmetic.hpp>

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

template<typename T>
class BitStream
{
public:
	/// Constructs new BitStream object from buffer.
	explicit BitStream(std::span<T> buffer) :
		m_buffer(buffer),
		m_position(buffer.begin()),
		m_bitsLeft(8)
	{}

	/// Skips specified number of bits from stream.
	void skip(int count)
	{
		m_bitsLeft -= count;

		while (m_bitsLeft <= 0)
		{
			++m_position;
			m_bitsLeft += 8;
		}
	}

	/// Pops number of bits from stream.
	/// Maximum 32 bits integers are supported.
	uint32_t pop(int count)
	{
		static const uint32_t MASK[33] =
		{ 0x00,
			0x01,		0x03,		0x07,		0x0f,
			0x1f,		0x3f,		0x7f,		0xff,
			0x1ff,		0x3ff,		0x7ff,		0xfff,
			0x1fff,		0x3fff,		0x7fff,		0xffff,
			0x1ffff,	0x3ffff,	0x7ffff,	0xfffff,
			0x1fffff,	0x3fffff,	0x7fffff,	0xffffff,
			0x1ffffff,	0x3ffffff,	0x7ffffff,	0xfffffff,
			0x1fffffff,	0x3fffffff,	0x7fffffff,	0xffffffff };
		int shift = 0;
		uint32_t result = 0;

		assert(count <= 32);

		while (count > 0)
		{
			if (m_position == m_buffer.end())
			{
				// Error in protocol.
				assert(0);
				break;
			}

			if ((shift = m_bitsLeft - count) >= 0)
			{
				// more in the buffer than requested
				result |= (*m_position >> shift) & MASK[count];
				m_bitsLeft -= count;
				if (m_bitsLeft == 0)
				{
					++m_position;
					m_bitsLeft = 8;
				}
				return result;
			}
			else
			{
				// less in the buffer than requested
				result |= (*m_position & MASK[m_bitsLeft]) << -shift;
				count -= m_bitsLeft;
				++m_position;
				m_bitsLeft = 8;
			}
		}

		return result;
	}

	std::span<T>::iterator position() const
	{
		// Ensure we are called only on aligned byte.
		assert(m_bitsLeft == 8);
		return m_position;
	}

	std::size_t bitPosition() const
	{
		return (m_position - m_buffer.begin()) * 8 + (8 - m_bitsLeft);
	}

private:
	std::span<T>			m_buffer;
	std::span<T>::iterator	m_position;
	int						m_bitsLeft;
};
}