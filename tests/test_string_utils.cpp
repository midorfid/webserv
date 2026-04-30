#include <gtest/gtest.h>
#include "StringUtils.hpp"

// ── trimLeftWhitespace ────────────────────────────────────────────────────

TEST(TrimLeft, RemovesLeadingSpaces) {
    std::string s = "   hello";
    StringUtils::trimLeftWhitespace(s);
    EXPECT_EQ(s, "hello");
}

TEST(TrimLeft, RemovesLeadingTabs) {
    std::string s = "\t\tworld";
    StringUtils::trimLeftWhitespace(s);
    EXPECT_EQ(s, "world");
}

TEST(TrimLeft, RemovesLeadingCRLF) {
    std::string s = "\r\n  value";
    StringUtils::trimLeftWhitespace(s);
    EXPECT_EQ(s, "value");
}

TEST(TrimLeft, AllWhitespaceBecomesEmpty) {
    std::string s = "   \t  ";
    StringUtils::trimLeftWhitespace(s);
    EXPECT_TRUE(s.empty());
}

TEST(TrimLeft, NoLeadingWhitespaceIsNoop) {
    std::string s = "hello world";
    StringUtils::trimLeftWhitespace(s);
    EXPECT_EQ(s, "hello world");
}

// ── trimRightWhitespace ───────────────────────────────────────────────────

TEST(TrimRight, RemovesTrailingSpaces) {
    std::string s = "hello   ";
    StringUtils::trimRightWhitespace(s);
    EXPECT_EQ(s, "hello");
}

TEST(TrimRight, RemovesTrailingCRLF) {
    std::string s = "header-value\r\n";
    StringUtils::trimRightWhitespace(s);
    EXPECT_EQ(s, "header-value");
}

TEST(TrimRight, AllWhitespaceBecomesEmpty) {
    std::string s = "  \r\n  ";
    StringUtils::trimRightWhitespace(s);
    EXPECT_TRUE(s.empty());
}

TEST(TrimRight, NoTrailingWhitespaceIsNoop) {
    std::string s = "hello";
    StringUtils::trimRightWhitespace(s);
    EXPECT_EQ(s, "hello");
}

// ── trimWhitespaces (both sides) ──────────────────────────────────────────

TEST(TrimBoth, StripsBothSides) {
    std::string s = "  \t hello world \r\n ";
    StringUtils::trimWhitespaces(s);
    EXPECT_EQ(s, "hello world");
}

TEST(TrimBoth, EmptyStringStaysEmpty) {
    std::string s = "";
    StringUtils::trimWhitespaces(s);
    EXPECT_TRUE(s.empty());
}

TEST(TrimBoth, SingleCharPreserved) {
    std::string s = " x ";
    StringUtils::trimWhitespaces(s);
    EXPECT_EQ(s, "x");
}

// ── stringToMethod ────────────────────────────────────────────────────────

TEST(StringToMethod, KnownMethodsMapCorrectly) {
    EXPECT_EQ(StringUtils::stringToMethod("GET"),    M_GET);
    EXPECT_EQ(StringUtils::stringToMethod("POST"),   M_POST);
    EXPECT_EQ(StringUtils::stringToMethod("PUT"),    M_PUT);
    EXPECT_EQ(StringUtils::stringToMethod("DELETE"), M_DELETE);
}

TEST(StringToMethod, UnknownMethodReturnsZero) {
    EXPECT_EQ(StringUtils::stringToMethod("PATCH"),   UNKNOWN);
    EXPECT_EQ(StringUtils::stringToMethod("HEAD"),    UNKNOWN);
    EXPECT_EQ(StringUtils::stringToMethod("OPTIONS"), UNKNOWN);
    EXPECT_EQ(StringUtils::stringToMethod(""),        UNKNOWN);
}

TEST(StringToMethod, CaseSensitive) {
    // The bitmask approach requires exact case — lowercase must not match
    EXPECT_EQ(StringUtils::stringToMethod("get"),  UNKNOWN);
    EXPECT_EQ(StringUtils::stringToMethod("Post"), UNKNOWN);
}

// ── split<char> ───────────────────────────────────────────────────────────

TEST(Split, SplitByChar) {
    auto parts = StringUtils::split<char>("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(Split, SingleElementNoDelimiter) {
    auto parts = StringUtils::split<char>("hello", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "hello");
}

TEST(Split, EmptyStringYieldsNoTokens) {
    // std::getline on an empty stream produces zero iterations
    auto parts = StringUtils::split<char>("", '/');
    EXPECT_TRUE(parts.empty());
}

TEST(Split, TrailingDelimiterIsNotEmitted) {
    // std::getline silently drops a trailing delimiter — no empty token
    auto parts = StringUtils::split<char>("a/b/", '/');
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
}
