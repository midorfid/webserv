#include <gtest/gtest.h>
#include "httpRequest.hpp"

// ── addHeader ─────────────────────────────────────────────────────────────

TEST(AddHeader, FirstAdd_ReturnsTrue) {
    HttpRequest req;
    EXPECT_TRUE(req.addHeader("x-custom", "value"));
    EXPECT_EQ(req.getHeader("x-custom"), "value");
}

TEST(AddHeader, NonSensitiveDuplicate_CommaFolds) {
    HttpRequest req;
    req.addHeader("accept", "text/html");
    req.addHeader("accept", "application/json");
    EXPECT_EQ(req.getHeader("accept"), "text/html, application/json");
}

TEST(AddHeader, NonSensitiveDuplicate_ReturnsTrueOnFold) {
    HttpRequest req;
    req.addHeader("accept", "text/html");
    EXPECT_TRUE(req.addHeader("accept", "application/json"));
}

// Each of the four sensitive headers must reject a duplicate with false.
// These are the HTTP request-smuggling protection paths.

TEST(AddHeader, DuplicateHost_ReturnsFalse) {
    HttpRequest req;
    EXPECT_TRUE(req.addHeader("host", "example.com"));
    EXPECT_FALSE(req.addHeader("host", "evil.com"));
    // First value must be preserved
    EXPECT_EQ(req.getHeader("host"), "example.com");
}

TEST(AddHeader, DuplicateContentLength_ReturnsFalse) {
    HttpRequest req;
    EXPECT_TRUE(req.addHeader("content-length", "5"));
    EXPECT_FALSE(req.addHeader("content-length", "9999"));
    EXPECT_EQ(req.getHeader("content-length"), "5");
}

TEST(AddHeader, DuplicateTransferEncoding_ReturnsFalse) {
    HttpRequest req;
    EXPECT_TRUE(req.addHeader("transfer-encoding", "chunked"));
    EXPECT_FALSE(req.addHeader("transfer-encoding", "identity"));
    EXPECT_EQ(req.getHeader("transfer-encoding"), "chunked");
}

TEST(AddHeader, DuplicateContentType_ReturnsFalse) {
    HttpRequest req;
    EXPECT_TRUE(req.addHeader("content-type", "text/html"));
    EXPECT_FALSE(req.addHeader("content-type", "application/json"));
    EXPECT_EQ(req.getHeader("content-type"), "text/html");
}

TEST(AddHeader, GetMissingHeader_ReturnsEmpty) {
    HttpRequest req;
    EXPECT_TRUE(req.getHeader("x-nonexistent").empty());
}

// ── isKeepAlive ───────────────────────────────────────────────────────────

TEST(KeepAlive, NoConnectionHeader_DefaultsToKeepAlive) {
    HttpRequest req;
    EXPECT_TRUE(req.isKeepAlive());
}

TEST(KeepAlive, ExplicitKeepAlive_IsTrue) {
    HttpRequest req;
    req.addHeader("connection", "keep-alive");
    EXPECT_TRUE(req.isKeepAlive());
}

TEST(KeepAlive, ConnectionClose_IsFalse) {
    HttpRequest req;
    req.addHeader("connection", "close");
    EXPECT_FALSE(req.isKeepAlive());
}

TEST(KeepAlive, AnyUnrecognisedValue_IsFalse) {
    // Any value other than exactly "keep-alive" or "" is treated as close.
    HttpRequest req;
    req.addHeader("connection", "Close");  // capital C — still not keep-alive
    EXPECT_FALSE(req.isKeepAlive());
}

TEST(KeepAlive, KeepAliveWithCapitalK_IsFalse) {
    // Documents case-sensitive comparison: "Keep-Alive" != "keep-alive".
    // A client sending the capitalised form would unexpectedly get a close.
    HttpRequest req;
    req.addHeader("connection", "Keep-Alive");
    EXPECT_FALSE(req.isKeepAlive());
}

// ── getCookie ─────────────────────────────────────────────────────────────

TEST(GetCookie, NoCookieHeader_ReturnsEmpty) {
    HttpRequest req;
    EXPECT_TRUE(req.getCookie("session_id").empty());
}

TEST(GetCookie, SingleCookie_ReturnsValue) {
    HttpRequest req;
    req.addHeader("cookie", "session_id=abc123");
    EXPECT_EQ(req.getCookie("session_id"), "abc123");
}

TEST(GetCookie, MultipleCookies_FirstFound) {
    HttpRequest req;
    req.addHeader("cookie", "a=1; b=2; c=3");
    EXPECT_EQ(req.getCookie("a"), "1");
}

TEST(GetCookie, MultipleCookies_MiddleFound) {
    HttpRequest req;
    req.addHeader("cookie", "a=1; b=2; c=3");
    EXPECT_EQ(req.getCookie("b"), "2");
}

TEST(GetCookie, MultipleCookies_LastFound) {
    HttpRequest req;
    req.addHeader("cookie", "a=1; b=2; c=3");
    EXPECT_EQ(req.getCookie("c"), "3");
}

TEST(GetCookie, MissingCookieName_ReturnsEmpty) {
    HttpRequest req;
    req.addHeader("cookie", "a=1; b=2");
    EXPECT_TRUE(req.getCookie("x").empty());
}

TEST(GetCookie, LeadingSpaceBeforeName_Skipped) {
    // Standard cookie format has a space after each semicolon: "a=1; b=2"
    HttpRequest req;
    req.addHeader("cookie", "a=1;  b=2");   // two spaces before b
    EXPECT_EQ(req.getCookie("b"), "2");
}

TEST(GetCookie, TrailingSpaceOnName_Trimmed) {
    // Key "session " with trailing space should match lookup "session"
    HttpRequest req;
    req.addHeader("cookie", "session =abc");
    EXPECT_EQ(req.getCookie("session"), "abc");
}

TEST(GetCookie, EmptyCookieValue_Returned) {
    HttpRequest req;
    req.addHeader("cookie", "token=");
    EXPECT_EQ(req.getCookie("token"), "");
}

TEST(GetCookie, CookieNameIsCaseSensitive) {
    HttpRequest req;
    req.addHeader("cookie", "Session=abc");
    EXPECT_TRUE(req.getCookie("session").empty());
    EXPECT_EQ(req.getCookie("Session"), "abc");
}
