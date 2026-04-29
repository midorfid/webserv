#include "Session.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

std::string SessionStore::generateId() {
    unsigned char buf[16];
    std::ifstream rng("/dev/urandom", std::ios::binary);
    rng.read(reinterpret_cast<char *>(buf), sizeof(buf));

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char b : buf)
        hex << std::setw(2) << static_cast<int>(b);
    return hex.str();
}

std::string SessionStore::getOrCreate(const std::string &sid, bool &out_is_new) {
    if (!sid.empty()) {
        auto it = _sessions.find(sid);
        if (it != _sessions.end() && it->second.expires > time(nullptr)) {
            it->second.expires = time(nullptr) + TTL; // sliding expiry
            out_is_new = false;
            return sid;
        }
    }
    std::string id = generateId();
    _sessions[id] = Session{{}, time(nullptr) + TTL};
    out_is_new = true;
    return id;
}

Session *SessionStore::get(const std::string &sid) {
    auto it = _sessions.find(sid);
    if (it == _sessions.end() || it->second.expires <= time(nullptr))
        return nullptr;
    return &it->second;
}

void SessionStore::destroy(const std::string &sid) {
    _sessions.erase(sid);
}

void SessionStore::purgeExpired() {
    time_t now = time(nullptr);
    for (auto it = _sessions.begin(); it != _sessions.end();) {
        if (it->second.expires <= now)
            it = _sessions.erase(it);
        else
            ++it;
    }
}
