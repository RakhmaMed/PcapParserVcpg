#ifndef PATTERN_SEEKER_H
#define PATTERN_SEEKER_H

#include <string_view>
#include <optional>

#include <cctype>
#include <cerrno>

#include <sstream>

namespace PatterSeekerNS
{

// MoveMode allows you to move the pointer before or after the desired data.
// By default, the pointer doesn't move.
enum MoveMode
{
    none,
    move_before,
    move_after,
};

// PatternSeeker is a class that is easy to use for parsing small strings with a predefined pattern.
// This class is just a display of the string passed in the constructor.
// Because of this, the object is very lightweight and can be copied at zero cost.
// Therefore, the responsibility for the lifetime of the original string falls on the developer.
// PatternSeeker also allows you to parse Json and XML.
// The author strongly recommends using appropriate parsers, however,
// this class is convenient because it doesn't make a copy of the data
// and allows you to easily and quickly parse a large structure and not use complex and heavy regular expressions.
// For better performance, this class offers the user to decide for himself when to move the pointer and when not.
class PatternSeeker
{
private:
    PatternSeeker(std::string_view str, const char* originalPointer);
    PatternSeeker();

public:
    PatternSeeker(std::string_view str);

    // Returns the size of the displayed string
    size_t size() const;

    // Checks the string for emptiness
    bool isEmpty() const;

    // Checking for non-emptiness (for better readability)
    bool isNotEmpty() const;

    // Returns the string_view of the visible part
    std::string_view to_string_view() const;

    // Returns the string of the visible part
    std::string to_string() const;

    // check what next and move the pointer after `expected`
    // return true if found `expected`
    bool expect(std::string_view expected);

    // Check what next and don't move the pointer
    bool startsWith(std::string_view expected) const;

    // Find the `expected` string and move the pointer after `expected`
    bool to(std::string_view expected, MoveMode mode=none);

    // Extract data `from` and `to` the desired strings.
    PatternSeeker extract(std::string_view from, std::string_view to, MoveMode mode=none);

    // Extract data from current position and `to` the desired strings.
    PatternSeeker extract(std::string_view to, MoveMode mode=none);

    // Extract data from current position `to` the desired symbols.
    PatternSeeker extractUntilOneOf(std::string_view to, MoveMode mode=none);

    // to avoid implicit convertion
    PatternSeeker extract(char) = delete;

    // Extracts data from one character to another.
    // However, if another `start` character is founded during the search for the `end` character,
    // the search continues until the corresponding `end` character is found.
    // It is convenient to search for objects in curly or square brackets.
    PatternSeeker extract(char start, char end, MoveMode mode=none);

    // Extracts the required number of characters
    PatternSeeker extract(size_t size, MoveMode mode=none);

    // to avoid implicit convertion
    void skip(char n) = delete;
    
    // Move the pointer and skip `n` elements
    void skip(size_t n);

    // Parses an unsigned number and shifts the pointer.
    // An empty boost::optional is returned on failure
    std::optional<uint64_t> takeUInt64();
    
    // An auxiliary number parsing method
    // that takes the default value and returns it in case of failure.
    uint64_t takeUInt64(uint64_t def);

    // Parses a signed number and shifts the pointer.
    // An empty boost::optional is returned on failure
    std::optional<int64_t> takeInt64();

    // An auxiliary number parsing method
    // that takes the default value and returns it in case of failure.
    int64_t takeInt64(int64_t def);

    // Removes all whitespace characters
    void skipWhiteSpaces();

    // Returns Json data by its name, whether it is a string, a number, an array, or a new object.
    PatternSeeker getJsonProp(std::string prop);

    // Returns the contents of the XML tag
    PatternSeeker getXmlTagBody(std::string prop, MoveMode mode=none);

    // Returns the entire tag, including the tag name and its attributes
    PatternSeeker getXmlTag(std::string prop, MoveMode mode=none);

    // Returns the contents of the XML attribute
    PatternSeeker getXmlAttr(std::string prop);

    // useful to know where a string starts
    size_t getOriginalPosition();

    // useful to know how much we've moved on.
    // For example, buffer.commit(offset)
    size_t getOffset();

    // output to the stream
    friend std::ostream& operator<<(std::ostream& oss, const PatternSeeker& parser) {
        return oss << parser.to_string_view();
    }

 protected:
    std::string_view m_str;
    const char* m_originalPointer;
};

}

#endif