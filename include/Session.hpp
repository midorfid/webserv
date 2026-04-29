#pragma once

#include <string>
#include <unordered_map>
#include <ctime>

struct Session {
    std::unordered_map<std::string, std::string> data;
    time_t expires;
};

class SessionStore {
public:
    static constexpr int TTL = 1800; // 30 minutes

    // Returns the session ID to use.
    // Reads sid from the client's Cookie header value; creates a new session
    // if sid is empty, unknown, or expired.  Sets out_is_new=true when the
    // caller must emit a Set-Cookie header.
    std::string getOrCreate(const std::string &sid, bool &out_is_new);

    // Returns nullptr if the session is absent or expired.
    Session    *get(const std::string &sid);

    void        destroy(const std::string &sid);
    void        purgeExpired();

private:
    std::unordered_map<std::string, Session> _sessions;
    static std::string generateId();
};
