#pragma once

#include <string>
#include <vector>
#include "Location.hpp"
#include "SharedCtx.hpp"

class Config {
public:
    Config();
    ~Config();

    Config(const Config &other);
    Config &operator=(const Config &other);

    SharedContext &getSharedCtx() { return _shared_ctx; }
    const SharedContext &getSharedCtx() const { return _shared_ctx; }

    void setPort(int port) { _port = port; }
    void addLocation(const Location &loc) { _locations.push_back(loc); }
    const std::vector<Location> &getLocations() const { return _locations; }

    bool getPort(const std::string &, std::string &port_out) const {
        if (_port == 0) return false;
        port_out = std::to_string(_port);
        return true;
    }
    int  getKeepAliveTimer() const { return _shared_ctx._keepalive_timer; }
    void setLocCgi() {} // CGI flag is set per-location during parsing
    const std::vector<std::string> &getServerNames() const { return _shared_ctx.server_names; }

private:
    SharedContext _shared_ctx;
    std::vector<Location> _locations;
    int _port;
};
