#include <gtest/gtest.h>
#include "ParseRequest.hpp"
#include "httpRequest.hpp"

// Each test gets a fresh parser and request object via the fixture.
class HttpParserTest : public ::testing::Test {
protected:
    ParseRequest parser;
    HttpRequest  req;
};

// ── Request-line parsing ──────────────────────────────────────────────────

TEST_F(HttpParserTest, ValidGetHttp11) {
    std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_EQ(req.getMethod(),  "GET");
    EXPECT_EQ(req.getPath(),    "/index.html");
    EXPECT_EQ(req.getVersion(), "HTTP/1.1");
}

TEST_F(HttpParserTest, ValidGetHttp10) {
    std::string raw = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_EQ(req.getVersion(), "HTTP/1.0");
}

TEST_F(HttpParserTest, ValidPostRequest) {
    std::string raw = "POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_EQ(req.getMethod(), "POST");
    EXPECT_EQ(req.getPath(),   "/upload");
}

TEST_F(HttpParserTest, UnsupportedVersion_Http20) {
    std::string raw = "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), UnsupportedVersion);
}

TEST_F(HttpParserTest, UnsupportedVersion_Gibberish) {
    std::string raw = "GET / BLORP/9\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), UnsupportedVersion);
}

TEST_F(HttpParserTest, UrlTooLong) {
    // MAX_URL_LENGTH is 2048; a path of 2049 chars must be rejected
    std::string long_path(2049, 'a');
    std::string raw = "GET " + long_path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), UrlTooLong);
}

TEST_F(HttpParserTest, UrlAtExactLimit_IsAccepted) {
    // A path of exactly MAX_URL_LENGTH (2048) characters must pass
    std::string exact_path(2048, 'a');
    std::string raw = "GET " + exact_path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
}

// ── Query string parsing ──────────────────────────────────────────────────

TEST_F(HttpParserTest, QueryStringSeparatedFromPath) {
    std::string raw = "GET /search?q=hello&page=2 HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_EQ(req.getPath(),  "/search");
    EXPECT_EQ(req.getQuery(), "q=hello&page=2");
}

TEST_F(HttpParserTest, NoQueryString_QueryIsEmpty) {
    std::string raw = "GET /page HTTP/1.1\r\nHost: localhost\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_TRUE(req.getQuery().empty());
}

// ── Header parsing ────────────────────────────────────────────────────────

TEST_F(HttpParserTest, HeaderNamesAreLowercased) {
    std::string raw = "GET / HTTP/1.1\r\nContent-Type: text/html\r\nX-Custom: myvalue\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    // Parser must fold header names to lowercase
    EXPECT_EQ(req.getHeader("content-type"), "text/html");
    EXPECT_EQ(req.getHeader("x-custom"),     "myvalue");
}

TEST_F(HttpParserTest, HeaderValuesAreTrimmed) {
    std::string raw = "GET / HTTP/1.1\r\nHost:   localhost   \r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_EQ(req.getHeader("host"), "localhost");
}

TEST_F(HttpParserTest, UnderscoreHeadersAreIgnored) {
    // Headers with underscores are silently dropped (security measure against
    // CGI environment variable injection via HTTP_SOME_HEADER aliasing)
    std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\nX_Injected: evil\r\n\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), Okay);
    EXPECT_TRUE(req.getHeader("x_injected").empty());
}

TEST_F(HttpParserTest, MissingRequestLineReturnsError) {
    std::string raw = "\r\n\r\n";
    ParseResult r = parser.parseReqLineHeaders(raw, req);
    EXPECT_NE(r, Okay);
}

// ── HTTP request-smuggling protection ─────────────────────────────────────
//
// Sending a header twice is either a smuggling vector (Content-Length,
// Transfer-Encoding) or a Host spoofing attempt.  RFC 7230 §3.3.2 permits
// servers to reject such requests with 400 Bad Request.

TEST_F(HttpParserTest, DuplicateHost_ReturnsBadRequest) {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: legitimate.com\r\n"
        "Host: evil.com\r\n"
        "\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), BadRequest);
}

TEST_F(HttpParserTest, DuplicateContentLength_ReturnsBadRequest) {
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 9999\r\n"
        "\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), BadRequest);
}

TEST_F(HttpParserTest, DuplicateTransferEncoding_ReturnsBadRequest) {
    std::string raw =
        "POST /data HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Transfer-Encoding: identity\r\n"
        "\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), BadRequest);
}

TEST_F(HttpParserTest, DuplicateContentType_ReturnsBadRequest) {
    std::string raw =
        "POST /data HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: text/html\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    EXPECT_EQ(parser.parseReqLineHeaders(raw, req), BadRequest);
}
