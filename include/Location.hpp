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
    void setLimitExcept(const std::vector<std::string> &methods) { _limit_except = methods; }

    const std::string &getPath() const { return _path; }
    const SharedContext &getSharedCtx() const { return _shared_ctx; }
    const std::optional<std::vector<std::string>> &getLimitExcept() const { return _limit_except; }

private:
    std::string _path;
    SharedContext _shared_ctx;
    std::optional<std::vector<std::string>> _limit_except;
};
