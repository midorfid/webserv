#include <gtest/gtest.h>
#include <fstream>
#include <unistd.h>
#include "ParseConfig.hpp"
#include "StringUtils.hpp"

// Fixture: each test gets its own ParseConfig and a helper that writes a
// unique temp file.  Files are written to testing::TempDir() so they live
// in /tmp and are cleaned up by the OS after the process exits.
class ConfigParserTest : public ::testing::Test {
protected:
    std::string writeConfig(const std::string &content) {
        std::string dir = testing::TempDir();
        if (dir.empty() || dir.back() != '/') dir += '/';
        std::string path = dir + "ws_" +
                           std::to_string(getpid()) + "_" +
                           std::to_string(counter_++) + ".conf";
        std::ofstream f(path);
        EXPECT_TRUE(f.is_open()) << "Cannot open temp file: " << path;
        f << content;
        return path;
    }

    ParseConfig parser;
    int         counter_ = 0;
};

// ── Top-level structure ───────────────────────────────────────────────────

TEST_F(ConfigParserTest, MinimalServer_ReturnsOneVhost) {
    auto vhosts = parser.parse(writeConfig("server { listen 8080; }\n"));
    ASSERT_EQ(vhosts.size(), 1u);
}

TEST_F(ConfigParserTest, TwoServerBlocks_ReturnsBothVhosts) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; }\n"
        "server { listen 9090; }\n"
    ));
    ASSERT_EQ(vhosts.size(), 2u);
}

TEST_F(ConfigParserTest, EmptyFile_ReturnsEmptyVector) {
    auto vhosts = parser.parse(writeConfig(""));
    EXPECT_TRUE(vhosts.empty());
}

TEST_F(ConfigParserTest, CommentsAreStripped) {
    auto vhosts = parser.parse(writeConfig(
        "# top-level comment\n"
        "server {\n"
        "  listen 8080; # inline comment\n"
        "  root /var/www; # another\n"
        "}\n"
    ));
    ASSERT_EQ(vhosts.size(), 1u);
    EXPECT_EQ(vhosts[0].getSharedCtx().root, "/var/www");
}

// ── listen ────────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, Listen_PortParsed) {
    auto vhosts = parser.parse(writeConfig("server { listen 8080; }\n"));
    std::string port;
    EXPECT_TRUE(vhosts[0].getPort("", port));
    EXPECT_EQ(port, "8080");
}

TEST_F(ConfigParserTest, TwoServers_DifferentPorts) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; }\n"
        "server { listen 9090; }\n"
    ));
    std::string p0, p1;
    vhosts[0].getPort("", p0);
    vhosts[1].getPort("", p1);
    EXPECT_EQ(p0, "8080");
    EXPECT_EQ(p1, "9090");
}

// ── server_names ──────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, ServerNames_AllParsed) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  server_names example.org www.example.org;\n"
        "}\n"
    ));
    const auto &names = vhosts[0].getServerNames();
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "example.org");
    EXPECT_EQ(names[1], "www.example.org");
}

// ── root ──────────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, Root_Parsed) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; root /var/www/html; }\n"
    ));
    EXPECT_EQ(vhosts[0].getSharedCtx().root, "/var/www/html");
}

// ── index ─────────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, IndexFiles_MultipleEntries) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; index index.html index.htm; }\n"
    ));
    const auto &idx = vhosts[0].getSharedCtx().index_files;
    ASSERT_EQ(idx.size(), 2u);
    EXPECT_EQ(idx[0], "index.html");
    EXPECT_EQ(idx[1], "index.htm");
}

// ── autoindex ─────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, AutoindexOn_True) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; autoindex on; }\n"
    ));
    EXPECT_TRUE(vhosts[0].getSharedCtx().autoindex);
}

TEST_F(ConfigParserTest, AutoindexOff_False) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; autoindex off; }\n"
    ));
    EXPECT_FALSE(vhosts[0].getSharedCtx().autoindex);
}

// ── client_max_body_size ──────────────────────────────────────────────────

TEST_F(ConfigParserTest, ClientMaxBodySize_Parsed) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; client_max_body_size 5000000; }\n"
    ));
    EXPECT_EQ(vhosts[0].getSharedCtx().client_max_body_size, 5000000u);
}

// ── keepalive_timeout ─────────────────────────────────────────────────────

TEST_F(ConfigParserTest, KeepaliveTimeout_Parsed) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; keepalive_timeout 30; }\n"
    ));
    EXPECT_EQ(vhosts[0].getKeepAliveTimer(), 30);
}

// ── error_page ────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, ErrorPage_SingleCode) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; error_page 404 /404.html; }\n"
    ));
    const auto &ep = vhosts[0].getSharedCtx().error_pages;
    ASSERT_EQ(ep.count(404), 1u);
    EXPECT_EQ(ep.at(404), "/404.html");
}

TEST_F(ConfigParserTest, ErrorPage_MultipleCodesOneUrl) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; error_page 500 502 503 /50x.html; }\n"
    ));
    const auto &ep = vhosts[0].getSharedCtx().error_pages;
    EXPECT_EQ(ep.at(500), "/50x.html");
    EXPECT_EQ(ep.at(502), "/50x.html");
    EXPECT_EQ(ep.at(503), "/50x.html");
}

TEST_F(ConfigParserTest, ErrorPage_TwoDistinctCodes) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  error_page 404 /404.html;\n"
        "  error_page 500 /500.html;\n"
        "}\n"
    ));
    const auto &ep = vhosts[0].getSharedCtx().error_pages;
    EXPECT_EQ(ep.at(404), "/404.html");
    EXPECT_EQ(ep.at(500), "/500.html");
}

// ── return (redirect) ─────────────────────────────────────────────────────

TEST_F(ConfigParserTest, Return301_ParsedWithTarget) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; return 301 /new-path; }\n"
    ));
    const auto &redir = vhosts[0].getSharedCtx().redirect;
    ASSERT_TRUE(redir.has_value());
    EXPECT_EQ(redir->first,  301);
    EXPECT_EQ(redir->second, "/new-path");
}

TEST_F(ConfigParserTest, Return302_ParsedCorrectly) {
    auto vhosts = parser.parse(writeConfig(
        "server { listen 8080; return 302 /temp; }\n"
    ));
    ASSERT_TRUE(vhosts[0].getSharedCtx().redirect.has_value());
    EXPECT_EQ(vhosts[0].getSharedCtx().redirect->first, 302);
}

// ── allow_cgi ─────────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, AllowCgi_SetsFlag) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location /cgi { allow_cgi .py; }\n"
        "}\n"
    ));
    ASSERT_EQ(vhosts[0].getLocations().size(), 1u);
    EXPECT_TRUE(vhosts[0].getLocations()[0].getSharedCtx().allow_cgi);
}

// ── location blocks ───────────────────────────────────────────────────────

TEST_F(ConfigParserTest, Location_PathParsed) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location /uploads { autoindex on; }\n"
        "}\n"
    ));
    ASSERT_EQ(vhosts[0].getLocations().size(), 1u);
    EXPECT_EQ(vhosts[0].getLocations()[0].getPath(), "/uploads");
}

TEST_F(ConfigParserTest, Location_InheritsServerRoot) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  root /srv/www;\n"
        "  location / { autoindex on; }\n"
        "}\n"
    ));
    // Locations are initialised with a copy of the server SharedContext
    EXPECT_EQ(vhosts[0].getLocations()[0].getSharedCtx().root, "/srv/www");
}

TEST_F(ConfigParserTest, Location_RootOverridesServer) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  root /srv/www;\n"
        "  location /static { root /srv/static; }\n"
        "}\n"
    ));
    EXPECT_EQ(vhosts[0].getLocations()[0].getSharedCtx().root, "/srv/static");
    // Server-level root is unchanged
    EXPECT_EQ(vhosts[0].getSharedCtx().root, "/srv/www");
}

TEST_F(ConfigParserTest, Location_ServerRootUnchangedAfterLocationOverride) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  root /srv/www;\n"
        "  location /a { root /srv/a; }\n"
        "  location /b { root /srv/b; }\n"
        "}\n"
    ));
    EXPECT_EQ(vhosts[0].getLocations()[0].getSharedCtx().root, "/srv/a");
    EXPECT_EQ(vhosts[0].getLocations()[1].getSharedCtx().root, "/srv/b");
    // Server ctx was only copied into each location; the original is intact
    EXPECT_EQ(vhosts[0].getSharedCtx().root, "/srv/www");
}

TEST_F(ConfigParserTest, MultipleLocations_AllPresent) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location / { autoindex off; }\n"
        "  location /api { autoindex off; }\n"
        "  location /static { autoindex on; }\n"
        "}\n"
    ));
    EXPECT_EQ(vhosts[0].getLocations().size(), 3u);
}

// ── limit_except ──────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, LimitExcept_SingleMethod_Bitmask) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location /ro { limit_except GET { deny all; } }\n"
        "}\n"
    ));
    const auto &limit = vhosts[0].getLocations()[0].getLimitExcept();
    ASSERT_TRUE(limit.has_value());
    EXPECT_EQ(*limit & M_GET,    M_GET);
    EXPECT_EQ(*limit & M_POST,   0);
    EXPECT_EQ(*limit & M_DELETE, 0);
}

TEST_F(ConfigParserTest, LimitExcept_MultiMethod_BitmaskOred) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location /rw { limit_except GET POST { deny all; } }\n"
        "}\n"
    ));
    const auto &limit = vhosts[0].getLocations()[0].getLimitExcept();
    ASSERT_TRUE(limit.has_value());
    EXPECT_EQ(*limit & M_GET,    M_GET);
    EXPECT_EQ(*limit & M_POST,   M_POST);
    EXPECT_EQ(*limit & M_DELETE, 0);
}

TEST_F(ConfigParserTest, NoLimitExcept_OptionalIsEmpty) {
    auto vhosts = parser.parse(writeConfig(
        "server {\n"
        "  listen 8080;\n"
        "  location / { autoindex on; }\n"
        "}\n"
    ));
    EXPECT_FALSE(vhosts[0].getLocations()[0].getLimitExcept().has_value());
}

// ── Global directives ─────────────────────────────────────────────────────

TEST_F(ConfigParserTest, Global_AccessLogStored) {
    parser.parse(writeConfig(
        "access_log /tmp/access.log;\n"
        "server { listen 8080; }\n"
    ));
    EXPECT_EQ(parser.getGlobal().access_log, "/tmp/access.log");
}

TEST_F(ConfigParserTest, Global_ErrorLogStored) {
    parser.parse(writeConfig(
        "error_log /tmp/error.log;\n"
        "server { listen 8080; }\n"
    ));
    EXPECT_EQ(parser.getGlobal().error_log, "/tmp/error.log");
}

TEST_F(ConfigParserTest, Global_LogsResetBetweenParses) {
    // First parse sets access_log; second parse has no access_log directive
    parser.parse(writeConfig("access_log /tmp/a.log;\nserver { listen 8080; }\n"));
    parser.parse(writeConfig("server { listen 9090; }\n"));
    EXPECT_TRUE(parser.getGlobal().access_log.empty());
}

TEST_F(ConfigParserTest, Global_BothLogsAbsent_DefaultsEmpty) {
    parser.parse(writeConfig("server { listen 8080; }\n"));
    EXPECT_TRUE(parser.getGlobal().access_log.empty());
    EXPECT_TRUE(parser.getGlobal().error_log.empty());
}

// ── Error handling ────────────────────────────────────────────────────────

TEST_F(ConfigParserTest, UnknownServerDirective_Throws) {
    EXPECT_THROW(
        parser.parse(writeConfig("server { listen 8080; frobnicator on; }\n")),
        std::runtime_error
    );
}

TEST_F(ConfigParserTest, UnknownGlobalToken_Throws) {
    EXPECT_THROW(
        parser.parse(writeConfig("garbage;\nserver { listen 8080; }\n")),
        std::runtime_error
    );
}

TEST_F(ConfigParserTest, NonexistentFile_Throws) {
    EXPECT_THROW(
        parser.parse("/nonexistent/path/webserv_test.conf"),
        std::runtime_error
    );
}
