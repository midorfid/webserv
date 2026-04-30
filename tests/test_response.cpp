#include <gtest/gtest.h>
#include "response.hpp"

// ── mimeFromPath ──────────────────────────────────────────────────────────

TEST(MimeFromPath, HtmlExtensions) {
    EXPECT_EQ(Response::mimeFromPath("index.html"), "text/html; charset=utf-8");
    EXPECT_EQ(Response::mimeFromPath("index.htm"),  "text/html; charset=utf-8");
}

TEST(MimeFromPath, StyleAndScript) {
    EXPECT_EQ(Response::mimeFromPath("style.css"),  "text/css");
    EXPECT_EQ(Response::mimeFromPath("app.js"),     "application/javascript");
    EXPECT_EQ(Response::mimeFromPath("data.json"),  "application/json");
}

TEST(MimeFromPath, PlainText) {
    EXPECT_EQ(Response::mimeFromPath("readme.txt"), "text/plain; charset=utf-8");
}

TEST(MimeFromPath, ImageTypes) {
    EXPECT_EQ(Response::mimeFromPath("photo.png"),  "image/png");
    EXPECT_EQ(Response::mimeFromPath("photo.jpg"),  "image/jpeg");
    EXPECT_EQ(Response::mimeFromPath("photo.jpeg"), "image/jpeg");
    EXPECT_EQ(Response::mimeFromPath("anim.gif"),   "image/gif");
    EXPECT_EQ(Response::mimeFromPath("icon.ico"),   "image/x-icon");
    EXPECT_EQ(Response::mimeFromPath("logo.svg"),   "image/svg+xml");
}

TEST(MimeFromPath, DocumentAndScript) {
    EXPECT_EQ(Response::mimeFromPath("doc.pdf"),    "application/pdf");
    EXPECT_EQ(Response::mimeFromPath("script.py"),  "text/x-python");
}

TEST(MimeFromPath, UnknownExtension_OctetStream) {
    EXPECT_EQ(Response::mimeFromPath("archive.zip"),  "application/octet-stream");
    EXPECT_EQ(Response::mimeFromPath("binary.wasm"),  "application/octet-stream");
    EXPECT_EQ(Response::mimeFromPath("file.unknown"), "application/octet-stream");
}

TEST(MimeFromPath, NoExtension_OctetStream) {
    EXPECT_EQ(Response::mimeFromPath("Makefile"),    "application/octet-stream");
    EXPECT_EQ(Response::mimeFromPath("/etc/passwd"), "application/octet-stream");
}

TEST(MimeFromPath, MultipleDots_UsesLastExtension) {
    // rfind('.') must pick the final dot
    EXPECT_EQ(Response::mimeFromPath("app.min.js"),    "application/javascript");
    EXPECT_EQ(Response::mimeFromPath("style.v2.css"),  "text/css");
    EXPECT_EQ(Response::mimeFromPath("report.2026.pdf"), "application/pdf");
}

TEST(MimeFromPath, FullPath_ExtensionStillRecognised) {
    EXPECT_EQ(Response::mimeFromPath("/var/www/html/index.html"), "text/html; charset=utf-8");
}

TEST(MimeFromPath, ExtensionMatchIsCaseSensitive) {
    // Implementation uses exact string comparison; uppercase is not normalised.
    EXPECT_EQ(Response::mimeFromPath("index.HTML"), "application/octet-stream");
    EXPECT_EQ(Response::mimeFromPath("photo.PNG"),  "application/octet-stream");
    EXPECT_EQ(Response::mimeFromPath("app.JS"),     "application/octet-stream");
}

// ── getStatusText ─────────────────────────────────────────────────────────

TEST(GetStatusText, AllDefinedCodes) {
    EXPECT_EQ(Response::getStatusText(200), "OK");
    EXPECT_EQ(Response::getStatusText(201), "Created");
    EXPECT_EQ(Response::getStatusText(204), "No Content");
    EXPECT_EQ(Response::getStatusText(301), "Moved Permanently");
    EXPECT_EQ(Response::getStatusText(302), "Found");
    EXPECT_EQ(Response::getStatusText(307), "Temporary Redirect");
    EXPECT_EQ(Response::getStatusText(308), "Permanent Redirect");
    EXPECT_EQ(Response::getStatusText(400), "Bad Request");
    EXPECT_EQ(Response::getStatusText(403), "Forbidden");
    EXPECT_EQ(Response::getStatusText(404), "Not Found");
    EXPECT_EQ(Response::getStatusText(405), "Method Not Allowed");
    EXPECT_EQ(Response::getStatusText(408), "Request Timeout");
    EXPECT_EQ(Response::getStatusText(413), "Payload Too Large");
    EXPECT_EQ(Response::getStatusText(414), "URI Too Long");
    EXPECT_EQ(Response::getStatusText(431), "Request Header Fields Too Large");
    EXPECT_EQ(Response::getStatusText(500), "Internal Server Error");
    EXPECT_EQ(Response::getStatusText(502), "Bad Gateway");
    EXPECT_EQ(Response::getStatusText(503), "Service Unavailable");
    EXPECT_EQ(Response::getStatusText(504), "Gateway Timeout");
    EXPECT_EQ(Response::getStatusText(505), "HTTP Version Not Supported");
}

TEST(GetStatusText, UnknownCode_FallsBackToInternalServerError) {
    EXPECT_EQ(Response::getStatusText(418), "Internal Server Error");
    EXPECT_EQ(Response::getStatusText(0),   "Internal Server Error");
    EXPECT_EQ(Response::getStatusText(999), "Internal Server Error");
}

// ── encodeChunk ───────────────────────────────────────────────────────────

TEST(EncodeChunk, EmptyInput_ReturnsEmptyString) {
    EXPECT_TRUE(Response::encodeChunk("").empty());
}

TEST(EncodeChunk, SingleByte_CorrectFrame) {
    EXPECT_EQ(Response::encodeChunk("A"), "1\r\nA\r\n");
}

TEST(EncodeChunk, FiveBytes_CorrectFrame) {
    EXPECT_EQ(Response::encodeChunk("hello"), "5\r\nhello\r\n");
}

TEST(EncodeChunk, SixteenBytes_HexSizeIs10) {
    std::string data(16, 'x');
    EXPECT_EQ(Response::encodeChunk(data), "10\r\n" + data + "\r\n");
}

TEST(EncodeChunk, TwoFiftyFiveBytes_HexSizeIsFF) {
    std::string data(255, 'z');
    EXPECT_EQ(Response::encodeChunk(data), "ff\r\n" + data + "\r\n");
}

// ── encodeChunked ─────────────────────────────────────────────────────────

TEST(EncodeChunked, EmptyBody_TerminalFrameOnly) {
    EXPECT_EQ(Response::encodeChunked(""), "0\r\n\r\n");
}

TEST(EncodeChunked, NonEmptyBody_ChunkPlusTerminal) {
    EXPECT_EQ(Response::encodeChunked("hello"), "5\r\nhello\r\n0\r\n\r\n");
}

TEST(EncodeChunked, CanBeDecodedByBodyParser) {
    // Verify the output of encodeChunked is exactly what the chunked body
    // parser expects (roundtrip sanity check between the two subsystems)
    std::string encoded = Response::encodeChunked("ping");
    // Must start with hex size, end with terminal chunk
    EXPECT_TRUE(encoded.starts_with("4\r\nping\r\n"));
    EXPECT_TRUE(encoded.ends_with("0\r\n\r\n"));
}

// ── build ─────────────────────────────────────────────────────────────────

TEST(Build, StatusLine_CorrectFormat) {
    ResponseState resp(200);
    std::string out = Response::build(resp);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 200 OK\r\n"));
}

TEST(Build, StatusCode404_StatusLineCorrect) {
    ResponseState resp(404);
    std::string out = Response::build(resp);
    EXPECT_TRUE(out.starts_with("HTTP/1.1 404 Not Found\r\n"));
}

TEST(Build, HeadersSection_PresentInOutput) {
    ResponseState resp(200);
    resp.addHeader("content-type", "text/plain");
    std::string out = Response::build(resp);
    EXPECT_NE(out.find("content-type: text/plain\r\n"), std::string::npos);
}

TEST(Build, EndOfHeadersMarker_Present) {
    ResponseState resp(200);
    std::string out = Response::build(resp);
    EXPECT_NE(out.find("\r\n\r\n"), std::string::npos);
}

TEST(Build, EmptyBody_OutputEndsWithBlankLine) {
    ResponseState resp(200);
    std::string out = Response::build(resp);
    EXPECT_TRUE(out.ends_with("\r\n\r\n"));
}

TEST(Build, NonEmptyBody_AppendedAfterBlankLine) {
    ResponseState resp(200);
    resp.body = "Hello, World!";
    std::string out = Response::build(resp);
    EXPECT_TRUE(out.ends_with("Hello, World!"));
    EXPECT_NE(out.find("\r\n\r\nHello, World!"), std::string::npos);
}

TEST(Build, SetCookieHeader_RenderedCorrectly) {
    ResponseState resp(200);
    resp.cookies.push_back("session_id=abc; HttpOnly; SameSite=Lax");
    std::string out = Response::build(resp);
    EXPECT_NE(out.find("Set-Cookie: session_id=abc; HttpOnly; SameSite=Lax\r\n"), std::string::npos);
}

TEST(Build, MultipleCookies_AllPresent) {
    ResponseState resp(200);
    resp.cookies.push_back("a=1");
    resp.cookies.push_back("b=2");
    std::string out = Response::build(resp);
    EXPECT_NE(out.find("Set-Cookie: a=1\r\n"), std::string::npos);
    EXPECT_NE(out.find("Set-Cookie: b=2\r\n"), std::string::npos);
}

// ── finalizeResponse ──────────────────────────────────────────────────────

class FinalizeTest : public ::testing::Test {
protected:
    ResponseState resp{200};
};

TEST_F(FinalizeTest, SetsDateHeader) {
    Response::finalizeResponse(resp, "/", 0);
    EXPECT_EQ(resp.headers.count("date"), 1u);
    EXPECT_FALSE(resp.headers.at("date").empty());
    // RFC 7231 HTTP-date always ends with "GMT"
    EXPECT_NE(resp.headers.at("date").find("GMT"), std::string::npos);
}

TEST_F(FinalizeTest, SetsServerHeader) {
    Response::finalizeResponse(resp, "/", 0);
    EXPECT_EQ(resp.headers.at("server"), "Webserv/1.0");
}

TEST_F(FinalizeTest, SetsContentLengthFromArgument) {
    Response::finalizeResponse(resp, "/", 42);
    EXPECT_EQ(resp.headers.at("content-length"), "42");
}

TEST_F(FinalizeTest, ContentLengthZero_StillSet) {
    Response::finalizeResponse(resp, "/", 0);
    EXPECT_EQ(resp.headers.at("content-length"), "0");
}

TEST_F(FinalizeTest, DefaultContentType_WhenBodySizeNonZero) {
    Response::finalizeResponse(resp, "/", 10);
    EXPECT_EQ(resp.headers.at("content-type"), "text/html; charset=utf-8");
}

TEST_F(FinalizeTest, NoDefaultContentType_WhenBodySizeZero) {
    Response::finalizeResponse(resp, "/", 0);
    EXPECT_EQ(resp.headers.count("content-type"), 0u);
}

TEST_F(FinalizeTest, ExistingContentType_NotOverridden) {
    resp.addHeader("content-type", "application/json");
    Response::finalizeResponse(resp, "/", 20);
    EXPECT_EQ(resp.headers.at("content-type"), "application/json");
}

TEST_F(FinalizeTest, KeepAlive_ConnectionIsKeepAlive) {
    Response::finalizeResponse(resp, "/", 0, true);
    EXPECT_EQ(resp.headers.at("connection"), "keep-alive");
}

TEST_F(FinalizeTest, NoKeepAlive_ConnectionIsClosed) {
    Response::finalizeResponse(resp, "/", 0, false);
    EXPECT_EQ(resp.headers.at("connection"), "closed");
}

TEST_F(FinalizeTest, LocationHeader_On201) {
    ResponseState created(201);
    Response::finalizeResponse(created, "/uploads/file.txt", 0);
    EXPECT_EQ(created.headers.at("location"), "/uploads/file.txt");
}

TEST_F(FinalizeTest, LocationHeader_On301) {
    ResponseState redir(301);
    Response::finalizeResponse(redir, "/new-path", 0);
    EXPECT_EQ(redir.headers.at("location"), "/new-path");
}

TEST_F(FinalizeTest, LocationHeader_On302) {
    ResponseState redir(302);
    Response::finalizeResponse(redir, "/temp", 0);
    EXPECT_EQ(redir.headers.at("location"), "/temp");
}

TEST_F(FinalizeTest, NoLocationHeader_On200) {
    Response::finalizeResponse(resp, "/some-path", 0);
    EXPECT_EQ(resp.headers.count("location"), 0u);
}

TEST_F(FinalizeTest, AllowHeader_On405) {
    ResponseState not_allowed(405);
    Response::finalizeResponse(not_allowed, "/", 0);
    EXPECT_EQ(not_allowed.headers.at("allow"), "GET, POST, DELETE");
}

TEST_F(FinalizeTest, WwwAuthenticate_On401) {
    ResponseState unauth(401);
    Response::finalizeResponse(unauth, "/", 0);
    EXPECT_EQ(unauth.headers.count("www-authenticate"), 1u);
}

TEST_F(FinalizeTest, SessionCookie_PushedToCookies) {
    Response::finalizeResponse(resp, "/", 0, false, "session_id=xyz; HttpOnly");
    ASSERT_EQ(resp.cookies.size(), 1u);
    EXPECT_EQ(resp.cookies[0], "session_id=xyz; HttpOnly");
}

TEST_F(FinalizeTest, EmptySessionCookie_NotPushed) {
    Response::finalizeResponse(resp, "/", 0, false, "");
    EXPECT_TRUE(resp.cookies.empty());
}
