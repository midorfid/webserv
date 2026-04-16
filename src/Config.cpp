#include "Config.hpp"

Config::Config() : _port(0) {}
Config::~Config() {}

Config::Config(const Config &other) : _shared_ctx(other._shared_ctx), _locations(other._locations), _port(other._port) {}
Config &Config::operator=(const Config &other) {
    if (this != &other) {
        _shared_ctx = other._shared_ctx;
        _locations  = other._locations;
        _port       = other._port;
    }
    return *this;
}
