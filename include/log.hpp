#pragma once

#include <ctime>
#include <string>
#include <iostream>

#define ERRLOG 2
#define REGLOG 1

inline std::string getTime() {
    time_t  currtime = time(NULL);

    struct tm *local_time = localtime(&currtime);

    char buf[80];

    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_time);

    return std::string(buf);
}
/* @param logtype is either REGLOG or ERRLOG */
inline void    logTime(int logtype) {
    std::string curr_time = getTime();
    switch(logtype) {
        case REGLOG:
            std::cout << '[' << curr_time << ']';
            return;
        case ERRLOG:
            std::cerr << '[' << curr_time << ']';
            return;
        default:
            return;
    }
}
