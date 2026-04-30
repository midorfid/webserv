#pragma once

#include <string>
#include "Logger.hpp"

#define ERRLOG 2
#define REGLOG 1

inline void logTime(int logtype, const std::string& msg) {
    Logger::instance().log(
        logtype == ERRLOG ? Logger::Level::ERROR : Logger::Level::INFO,
        msg
    );
}
