#include "PatternSeeker.h"

#include <cctype>
#include <cerrno>


namespace PatterSeekerNS
{

static const char* EMPTY_STR = "";
static const std::string DOUBLE_QUOTE = "\"";

PatternSeeker::PatternSeeker(std::string_view str, const char* originalPointer)
    : m_str(str.data() ? str : EMPTY_STR)
    , m_originalPointer(originalPointer)
{}

PatternSeeker::PatternSeeker()
    : m_str(EMPTY_STR)
    , m_originalPointer(EMPTY_STR)
{}

PatternSeeker::PatternSeeker(std::string_view str)
    : m_str(str.data() ? str : EMPTY_STR)
    , m_originalPointer(m_str.data())
{}

size_t PatternSeeker::size() const
{
    return m_str.size();
}

bool PatternSeeker::isEmpty() const
{
    return size() == 0;
}

bool PatternSeeker::isNotEmpty() const
{
    return size() != 0;
}

std::string_view PatternSeeker::to_string_view() const
{
    return m_str;
}


std::string PatternSeeker::to_string() const
{
    return std::string{ m_str.data(), m_str.size() };
}

bool PatternSeeker::expect(std::string_view expected)
{
    if (m_str.starts_with(expected))
    {
        m_str.remove_prefix(expected.size());
        return true;
    }

    return false;
}

bool PatternSeeker::startsWith(std::string_view expected) const
{
    return m_str.starts_with(expected);
}

bool PatternSeeker::to(std::string_view expected, MoveMode mode)
{
    const size_t pos = m_str.find(expected);
    if (pos == std::string_view::npos)
    {
        return false;
    }

    switch (mode)
    {
    case move_before:
        m_str.remove_prefix(pos);
        break;
    case move_after:
    case none:
        m_str.remove_prefix(pos + expected.size());
        break;
    }

    return true;
}

PatternSeeker PatternSeeker::extract(std::string_view from, std::string_view to, MoveMode mode)
{
    auto startIt = m_str.find(from);
    if (startIt == std::string::npos)
    {
        return {};
    }
    startIt += from.size();

    const auto endIt = m_str.find(to, startIt);
    if (endIt == std::string::npos) {
        return {};
    }

    auto substr = m_str.substr(startIt, endIt - startIt);

    switch (mode)
    {
    case move_before:
        m_str.remove_prefix(startIt - from.size());
        break;
    case move_after:
        m_str.remove_prefix(endIt + to.size());
        break;
    }
        
    return PatternSeeker(substr, m_originalPointer);
}

PatternSeeker PatternSeeker::extract(std::string_view to, MoveMode mode)
{
    const auto endIt = m_str.find(to);
    if (endIt == std::string::npos)
    {
        return {};
    }

    auto substr = m_str.substr(0, endIt);

    if (mode == move_after)
        m_str.remove_prefix(endIt + to.size());

    return PatternSeeker(substr, m_originalPointer);
}

PatternSeeker PatternSeeker::extractUntilOneOf(std::string_view to, MoveMode mode)
{
    const auto endIt = m_str.find_first_of(to);
    if (endIt == std::string::npos)
    {
        return {};
    }

    auto substr = m_str.substr(0, endIt);

    if (mode == move_after)
        m_str.remove_prefix(endIt + to.size());

    return PatternSeeker(substr, m_originalPointer);
}

PatternSeeker PatternSeeker::extract(char start, char end, MoveMode mode)
{
    size_t startIndex = m_str.find(start);
    if (startIndex == -1)
        return {};

    size_t endIndex = startIndex + 1;
    size_t count = 1;
    while (endIndex < m_str.size())
    {
        char ch = m_str[endIndex];
        endIndex += 1;
        count += ch == start;
        count -= ch == end;
        if (ch == end && count != 1)
            break;
    }

    if (count > 1)
        return {};

    auto substr = m_str.substr(startIndex, endIndex - startIndex);

    switch (mode)
    {
    case move_before:
        m_str.remove_prefix(startIndex);
        break;
    case move_after:
        m_str.remove_prefix(endIndex);
        break;
    }

    return PatternSeeker{substr, m_originalPointer};
}

PatternSeeker PatternSeeker::extract(size_t size, MoveMode mode)
{
    auto substr = m_str.substr(0, size);

    if (mode == move_after)
        m_str.remove_prefix(size);

    return PatternSeeker{substr, m_originalPointer};
}

void PatternSeeker::skip(size_t n)
{
    m_str.remove_prefix(n);
}

std::optional<uint64_t> PatternSeeker::takeUInt64()
{
    errno = 0; // thread safe
    char* end = nullptr;
    const auto result = std::strtoull(m_str.data(), &end, 10);
    bool isError = errno || end == m_str.data();
    m_str.remove_prefix(end - m_str.data());
    if (isError)
        return {};
    return result;
}
    
uint64_t PatternSeeker::takeUInt64(uint64_t def)
{
    auto res = takeUInt64();
    if (!res)
        return def;
    return *res;
}

std::optional<int64_t> PatternSeeker::takeInt64()
{
    errno = 0; // thread safe
    char* end = nullptr;
    const auto result = std::strtoll(m_str.data(), &end, 10);
    bool isError = errno || end == m_str.data();
    m_str.remove_prefix(end - m_str.data());
    if (isError)
        return {};
    return result;
}

int64_t PatternSeeker::takeInt64(int64_t def)
{
    auto res = takeInt64();
    if (!res)
        return def;
    return *res;
}

void PatternSeeker::skipWhiteSpaces()
{
    while (m_str.size() && std::isspace(static_cast<unsigned char>(m_str[0]))) {
        m_str.remove_prefix(1);
    }
}

PatternSeeker PatternSeeker::getJsonProp(std::string prop)
{
    auto copy = *this;
    if (!copy.to(DOUBLE_QUOTE + prop + DOUBLE_QUOTE))
        return {};

    copy.skipWhiteSpaces();
    if (!copy.expect(":"))
        return {};

    copy.skipWhiteSpaces();
    if (copy.expect(DOUBLE_QUOTE))
        return copy.extract(DOUBLE_QUOTE);

    if (copy.startsWith("["))
        return copy.extract('[', ']');

    if (copy.startsWith("{"))
        return copy.extract('{', '}');

    return copy.extractUntilOneOf(", \r\n]}");
}

PatternSeeker PatternSeeker::getXmlTagBody(std::string prop, MoveMode mode)
{
    auto res = getXmlTag(prop, mode);
    if (res.isEmpty())
        return {};
    auto startPos = res.m_str.find('>') + 1;
    auto endPos = res.m_str.rfind("</" + prop + ">");
    return res.m_str.substr(startPos, endPos - startPos);
}

PatternSeeker PatternSeeker::getXmlTag(std::string prop, MoveMode mode)
{
    auto startTag = "<" + prop;
    size_t startPos = m_str.find(startTag);
    if (startPos == -1)
        return {};

    auto endTag = "</" + prop + ">";
    size_t endPos = m_str.find(endTag, startPos + startTag.size());

    auto substr = m_str.substr(startPos, endPos + endTag.size() - startPos);
        
    switch (mode)
    {
    case move_before:
        m_str.remove_prefix(startPos);
        break;
    case move_after:
        m_str.remove_prefix(endPos + endTag.size());
        break;
    }

    return PatternSeeker{substr, m_originalPointer};
}

PatternSeeker PatternSeeker::getXmlAttr(std::string prop)
{
    auto copy = *this;
    if (!copy.to(prop))
        return {};
    copy.skipWhiteSpaces();
    copy.expect("=");
    copy.skipWhiteSpaces();
    return copy.extract(DOUBLE_QUOTE, DOUBLE_QUOTE);
}

size_t PatternSeeker::getOriginalPosition()
{
    return m_str.data() - m_originalPointer;
}

size_t PatternSeeker::getOffset()
{
    return m_str.data() - m_originalPointer;
}


}
