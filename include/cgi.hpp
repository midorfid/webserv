#pragma once

#include "RouteRequest.hpp"

struct CgiInfo {
    int             write_fd;
    int             read_fd;
    pid_t           child_pid;
    time_t          cgi_start;    // wall-clock time the child was launched
    std::string     output_buf;
    bool            headers_sent; // headers parsed and forwarded to client
    bool            is_chunked;   // response uses Transfer-Encoding: chunked

    explicit CgiInfo(bool = false) { reset(); }

    int  &getReadfd()        { return read_fd; }
    int   getReadfd()  const { return read_fd; }
    int  &getWritefd()       { return write_fd; }
    int   getWritefd() const { return write_fd; }
    bool  isCgi()      const { return child_pid != -1 || read_fd != -1 || write_fd != -1; }

    void    addFds(const ResolvedAction &action) {
        read_fd   = action.cgi_fds.first;
        write_fd  = action.cgi_fds.second;
        child_pid = action.child_pid;
        cgi_start = time(nullptr);
    }

    void    reset() {
        write_fd     = -1;
        read_fd      = -1;
        child_pid    = -1;
        cgi_start    = 0;
        headers_sent = false;
        is_chunked   = false;
        output_buf.clear();
    }
};
