#include <gtest/gtest.h>
#include "ParseRequest.hpp"
#include "httpRequest.hpp"

// GTest creates a fresh fixture for each TEST_F, so parser and req are
// always in their default-constructed state at the start of every test.
class BodyParserTest : public ::testing::Test {
protected:
    ParseRequest parser;
    HttpRequest  req;
};

// ── Content-Length path ───────────────────────────────────────────────────

TEST_F(BodyParserTest, CL_NoHeader_TreatedAsZero) {
    // strtoul("") == 0, so body_size == 0 → complete immediately
    EXPECT_EQ(parser.parseBody("", req), RequestComplete);
    EXPECT_TRUE(req.getBody().empty());
}

TEST_F(BodyParserTest, CL_Zero_CompleteImmediately) {
    req.addHeader("content-length", "0");
    EXPECT_EQ(parser.parseBody("", req), RequestComplete);
    EXPECT_TRUE(req.getBody().empty());
}

TEST_F(BodyParserTest, CL_FullBodyArrives_Complete) {
    req.addHeader("content-length", "5");
    EXPECT_EQ(parser.parseBody("hello", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "hello");
}

TEST_F(BodyParserTest, CL_PartialBodyArrives_Incomplete) {
    req.addHeader("content-length", "10");
    EXPECT_EQ(parser.parseBody("partial", req), RequestIncomplete);
}

TEST_F(BodyParserTest, CL_EmptyBodyWhenExpected_Incomplete) {
    req.addHeader("content-length", "5");
    EXPECT_EQ(parser.parseBody("", req), RequestIncomplete);
}

TEST_F(BodyParserTest, CL_ExactOneByte_Complete) {
    req.addHeader("content-length", "1");
    EXPECT_EQ(parser.parseBody("X", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "X");
}

TEST_F(BodyParserTest, CL_BinaryPayload_PreservedVerbatim) {
    // Ensures the body path does not misinterpret null bytes or high bytes
    const std::string binary = {'\x00', '\x01', '\xFF', '\xFE'};
    req.addHeader("content-length", "4");
    EXPECT_EQ(parser.parseBody(binary, req), RequestComplete);
    EXPECT_EQ(req.getBody(), binary);
}

// ── Chunked path ──────────────────────────────────────────────────────────
//
// Chunked frame format:  <hex-size>\r\n<data>\r\n  ...  0\r\n\r\n

TEST_F(BodyParserTest, Chunked_SingleChunk_Decoded) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("5\r\nhello\r\n0\r\n\r\n", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "hello");
}

TEST_F(BodyParserTest, Chunked_TwoChunks_Concatenated) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "hello world");
}

TEST_F(BodyParserTest, Chunked_ThreeChunks_Concatenated) {
    req.addHeader("transfer-encoding", "chunked");
    std::string data = "3\r\nfoo\r\n3\r\nbar\r\n3\r\nbaz\r\n0\r\n\r\n";
    EXPECT_EQ(parser.parseBody(data, req), RequestComplete);
    EXPECT_EQ(req.getBody(), "foobarbaz");
}

TEST_F(BodyParserTest, Chunked_TerminalChunkOnly_EmptyBody) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("0\r\n\r\n", req), RequestComplete);
    EXPECT_TRUE(req.getBody().empty());
}

TEST_F(BodyParserTest, Chunked_UppercaseHexSize_Accepted) {
    req.addHeader("transfer-encoding", "chunked");
    // 0xA == 10 bytes
    EXPECT_EQ(parser.parseBody("A\r\n0123456789\r\n0\r\n\r\n", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "0123456789");
}

TEST_F(BodyParserTest, Chunked_LowercaseHexSize_Accepted) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("a\r\n0123456789\r\n0\r\n\r\n", req), RequestComplete);
    EXPECT_EQ(req.getBody(), "0123456789");
}

// ── Chunked — incomplete inputs ───────────────────────────────────────────

TEST_F(BodyParserTest, Chunked_EmptyInput_Incomplete) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("", req), RequestIncomplete);
}

TEST_F(BodyParserTest, Chunked_SizeLineWithNoCRLF_Incomplete) {
    req.addHeader("transfer-encoding", "chunked");
    // No \r\n after hex size — can't determine where data starts
    EXPECT_EQ(parser.parseBody("5", req), RequestIncomplete);
}

TEST_F(BodyParserTest, Chunked_DataTruncatedMidChunk_Incomplete) {
    req.addHeader("transfer-encoding", "chunked");
    // Chunk says 5 bytes but only 3 bytes of data arrived
    EXPECT_EQ(parser.parseBody("5\r\nhel", req), RequestIncomplete);
}

TEST_F(BodyParserTest, Chunked_MissingTerminalChunk_Incomplete) {
    req.addHeader("transfer-encoding", "chunked");
    // Full chunk data present but no 0\r\n\r\n — still waiting
    EXPECT_EQ(parser.parseBody("5\r\nhello\r\n", req), RequestIncomplete);
}

// ── Chunked — malformed inputs ────────────────────────────────────────────

TEST_F(BodyParserTest, Chunked_InvalidHexSize_ReturnsError) {
    req.addHeader("transfer-encoding", "chunked");
    // 'Z' is not a valid hex digit
    EXPECT_EQ(parser.parseBody("ZZ\r\nhello\r\n0\r\n\r\n", req), Error);
}

TEST_F(BodyParserTest, Chunked_NonHexLetters_ReturnsError) {
    req.addHeader("transfer-encoding", "chunked");
    EXPECT_EQ(parser.parseBody("xyz\r\ndata\r\n0\r\n\r\n", req), Error);
}
