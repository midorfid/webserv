#include "Location.hpp"

Location::Location() {}
Location::~Location() {}
Location::Location(const Location &other) : _path(other._path), _shared_ctx(other._shared_ctx), _limit_except(other._limit_except) {}
Location &Location::operator=(const Location &other) {
    if (this != &other) {
        _path         = other._path;
        _shared_ctx   = other._shared_ctx;
        _limit_except = other._limit_except;
    }
    return *this;
}
