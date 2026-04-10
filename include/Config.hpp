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

private:
    SharedContext _shared_ctx;
    std::vector<Location> _locations;
    int _port;
};
