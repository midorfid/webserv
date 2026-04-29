#pragma once

#include <string>
#include <vector>
#include <optional>
#include "SharedCtx.hpp"
#include "StringUtils.hpp"

class Location {
public:
    Location();
    ~Location();
    Location(const Location &other);
    Location &operator=(const Location &other);

    void setSharedCtx(const SharedContext &ctx) { _shared_ctx = ctx; }
    void setPath(const std::string &path) { _path = path; }
    void setLimitExcept(int bitmask) { _limit_except = bitmask; }

    const std::string &getPath() const { return _path; }
    const SharedContext &getSharedCtx() const { return _shared_ctx; }
    const std::optional<int> &getLimitExcept() const { return _limit_except; }

private:
    std::string _path;
    SharedContext _shared_ctx;
    std::optional<int> _limit_except;
};
