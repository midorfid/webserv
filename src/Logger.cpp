#include "Logger.hpp"

#include <ctime>
#include <cstring>
#include <iostream>

static std::string currentTime() {
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return buf;
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& access_path, const std::string& error_path) {
    // std::ofstream handles /dev/stdout and /dev/stderr natively on Linux
    if (!access_path.empty()) {
        _access.open(access_path, std::ios::app);
        if (!_access.is_open())
            std::cerr << "[Logger] Cannot open access log: " << access_path << '\n';
    }
    if (!error_path.empty()) {
        _error.open(error_path, std::ios::app);
        if (!_error.is_open())
            std::cerr << "[Logger] Cannot open error log: " << error_path << '\n';
    }
}

void Logger::log(Level level, const std::string& msg) {
    bool is_error = (level == Level::ERROR || level == Level::FATAL);

    std::ostream& dest = is_error
        ? (_error.is_open() ? static_cast<std::ostream&>(_error) : std::cerr)
        : (_access.is_open() ? static_cast<std::ostream&>(_access) : std::cout);

    dest << '[' << currentTime() << "] " << msg << '\n';
    if (is_error)
        dest.flush();
}

void Logger::shutdown() {
    if (_access.is_open()) { _access.flush(); _access.close(); }
    if (_error.is_open())  { _error.flush();  _error.close(); }
}
