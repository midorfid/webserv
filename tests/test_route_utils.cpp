#include <gtest/gtest.h>
#include "RouteRequest.hpp"
#include "Config.hpp"
#include "Location.hpp"

// Build a Config pre-populated with locations at the given paths.
static Config makeConfig(const std::vector<std::string> &paths) {
    Config cfg;
    for (const auto &p : paths) {
        Location loc;
        loc.setPath(p);
        cfg.addLocation(loc);
    }
    return cfg;
}

// Build a Location with an optional limit_except bitmask.
static Location makeLocation(const std::string &path, int bitmask = -1) {
    Location loc;
    loc.setPath(path);
    if (bitmask >= 0)
        loc.setLimitExcept(bitmask);
    return loc;
}

// ── findBestLocationMatch ─────────────────────────────────────────────────

TEST(FindBestLocationMatch, NoLocations_ReturnsNull) {
    RouteRequest rr;
    Config cfg;
    EXPECT_EQ(rr.findBestLocationMatch(cfg, "/any"), nullptr);
}

TEST(FindBestLocationMatch, SingleLocation_ExactMatch) {
    RouteRequest rr;
    Config cfg = makeConfig({"/api"});
    const Location *loc = rr.findBestLocationMatch(cfg, "/api/users");
    ASSERT_NE(loc, nullptr);
    EXPECT_EQ(loc->getPath(), "/api");
}

TEST(FindBestLocationMatch, RootLocation_MatchesEverything) {
    RouteRequest rr;
    Config cfg = makeConfig({"/"});
    EXPECT_NE(rr.findBestLocationMatch(cfg, "/"),           nullptr);
    EXPECT_NE(rr.findBestLocationMatch(cfg, "/foo/bar"),    nullptr);
    EXPECT_NE(rr.findBestLocationMatch(cfg, "/deeply/nested/path"), nullptr);
}

TEST(FindBestLocationMatch, LongestPrefixWins) {
    RouteRequest rr;
    // Three competing prefixes for /api/v2/users
    Config cfg = makeConfig({"/", "/api", "/api/v2"});

    const Location *loc = rr.findBestLocationMatch(cfg, "/api/v2/users");
    ASSERT_NE(loc, nullptr);
    EXPECT_EQ(loc->getPath(), "/api/v2");  // longest match wins
}

TEST(FindBestLocationMatch, ExactLocationSelectedOverRoot) {
    RouteRequest rr;
    Config cfg = makeConfig({"/", "/static"});
    const Location *loc = rr.findBestLocationMatch(cfg, "/static/img.png");
    ASSERT_NE(loc, nullptr);
    EXPECT_EQ(loc->getPath(), "/static");
}

TEST(FindBestLocationMatch, NoMatchingPrefix_ReturnsNull) {
    RouteRequest rr;
    Config cfg = makeConfig({"/api", "/static"});
    // "/other" doesn't start with either prefix
    EXPECT_EQ(rr.findBestLocationMatch(cfg, "/other/path"), nullptr);
}

TEST(FindBestLocationMatch, PrefixMatchIsCharacterBased) {
    RouteRequest rr;
    // "/app" is a prefix of "/application" — documents that the router
    // uses pure string prefix semantics, not path-segment semantics.
    Config cfg = makeConfig({"/app"});
    EXPECT_NE(rr.findBestLocationMatch(cfg, "/application"), nullptr);
}

TEST(FindBestLocationMatch, EmptyUrl_MatchesRootOnly) {
    RouteRequest rr;
    Config cfg = makeConfig({"/", "/api"});
    const Location *loc = rr.findBestLocationMatch(cfg, "");
    // "" has no prefix of "/" (substr(0,1) != "/"), so root doesn't match
    EXPECT_EQ(loc, nullptr);
}

// ── checkLimitExcept ──────────────────────────────────────────────────────

TEST(CheckLimitExcept, NoLimitExcept_AllMethodsAllowed) {
    RouteRequest rr;
    Location loc = makeLocation("/");  // no setLimitExcept → optional empty
    EXPECT_TRUE(rr.checkLimitExcept("GET",    loc));
    EXPECT_TRUE(rr.checkLimitExcept("POST",   loc));
    EXPECT_TRUE(rr.checkLimitExcept("PUT",    loc));
    EXPECT_TRUE(rr.checkLimitExcept("DELETE", loc));
}

TEST(CheckLimitExcept, GetOnly_GetAllowed) {
    RouteRequest rr;
    Location loc = makeLocation("/ro", M_GET);
    EXPECT_TRUE(rr.checkLimitExcept("GET", loc));
}

TEST(CheckLimitExcept, GetOnly_PostDenied) {
    RouteRequest rr;
    Location loc = makeLocation("/ro", M_GET);
    EXPECT_FALSE(rr.checkLimitExcept("POST",   loc));
    EXPECT_FALSE(rr.checkLimitExcept("PUT",    loc));
    EXPECT_FALSE(rr.checkLimitExcept("DELETE", loc));
}

TEST(CheckLimitExcept, GetAndPost_BothAllowed) {
    RouteRequest rr;
    Location loc = makeLocation("/rw", M_GET | M_POST);
    EXPECT_TRUE(rr.checkLimitExcept("GET",  loc));
    EXPECT_TRUE(rr.checkLimitExcept("POST", loc));
}

TEST(CheckLimitExcept, GetAndPost_DeleteDenied) {
    RouteRequest rr;
    Location loc = makeLocation("/rw", M_GET | M_POST);
    EXPECT_FALSE(rr.checkLimitExcept("DELETE", loc));
    EXPECT_FALSE(rr.checkLimitExcept("PUT",    loc));
}

TEST(CheckLimitExcept, AllMethods_AllAllowed) {
    RouteRequest rr;
    Location loc = makeLocation("/all", M_GET | M_POST | M_PUT | M_DELETE);
    EXPECT_TRUE(rr.checkLimitExcept("GET",    loc));
    EXPECT_TRUE(rr.checkLimitExcept("POST",   loc));
    EXPECT_TRUE(rr.checkLimitExcept("PUT",    loc));
    EXPECT_TRUE(rr.checkLimitExcept("DELETE", loc));
}

TEST(CheckLimitExcept, UnknownMethod_DeniedWhenLimitSet) {
    RouteRequest rr;
    Location loc = makeLocation("/", M_GET);
    // stringToMethod returns UNKNOWN (0); 0 & M_GET == 0 → denied
    EXPECT_FALSE(rr.checkLimitExcept("PATCH",   loc));
    EXPECT_FALSE(rr.checkLimitExcept("OPTIONS", loc));
    EXPECT_FALSE(rr.checkLimitExcept("",        loc));
}

// ── setActionType ─────────────────────────────────────────────────────────

TEST(SetActionType, Put_SetsUploadFile) {
    RouteRequest rr;
    ResolvedAction action;
    action.type = ACTION_NONE;
    rr.setActionType(action, "PUT");
    EXPECT_EQ(action.type, ACTION_UPLOAD_FILE);
}

TEST(SetActionType, Delete_SetsDeleteFile) {
    RouteRequest rr;
    ResolvedAction action;
    action.type = ACTION_NONE;
    rr.setActionType(action, "DELETE");
    EXPECT_EQ(action.type, ACTION_DELETE_FILE);
}

TEST(SetActionType, Post_SetsPostUpload) {
    RouteRequest rr;
    ResolvedAction action;
    action.type = ACTION_NONE;
    rr.setActionType(action, "POST");
    EXPECT_EQ(action.type, ACTION_POST_UPLOAD);
}

TEST(SetActionType, Get_TypeUnchanged) {
    // GET has no side-effect in setActionType; type stays at whatever it was
    RouteRequest rr;
    ResolvedAction action;
    action.type = ACTION_NONE;
    rr.setActionType(action, "GET");
    EXPECT_EQ(action.type, ACTION_NONE);
}

TEST(SetActionType, UnknownMethod_TypeUnchanged) {
    RouteRequest rr;
    ResolvedAction action;
    action.type = ACTION_NONE;
    rr.setActionType(action, "PATCH");
    EXPECT_EQ(action.type, ACTION_NONE);
}
