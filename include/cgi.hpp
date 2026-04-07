#pragma once

#include "RouteRequest.hpp"

struct CgiInfo {
    int             write_fd;
    int             read_fd;
    
    pid_t           child_pid;
    
    std::string     output_buf;
    
    CgiInfo() { reset(); }

    void    addFds(const ResolvedAction &action) {
        read_fd = action.cgi_fds.first;
        write_fd = action.cgi_fds.second;
    }

    void    reset() {
        write_fd = -1;
        read_fd = -1;
        child_pid = -1;
        output_buf.clear();
    }
};
