#pragma once

#include <string>
#include <fstream>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR, FATAL };

    static Logger& instance();

    void init(const std::string& access_path, const std::string& error_path);
    void log(Level level, const std::string& msg);
    void shutdown();

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream _access;
    std::ofstream _error;
};
