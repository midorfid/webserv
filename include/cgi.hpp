#pragma once

#include "RouteRequest.hpp"

class CgiInfo {
    private:
        bool            _is_cgi;
        int             _write_fd;
        int             _read_fd;
        std::string     _write_buf;
    
    public:
        CgiInfo() : _is_cgi(false) {}
        CgiInfo(bool allowed) : _is_cgi(allowed) {}
        ~CgiInfo() {}

        CgiInfo(const CgiInfo &other) : _is_cgi(other._is_cgi), _write_fd(other._write_fd), _read_fd(other._read_fd), _write_buf(other._write_buf) {}
        CgiInfo &operator=(const CgiInfo &other) {
            if (&other != this) {
                _is_cgi = other._is_cgi;
                _write_fd = other._write_fd;
                _read_fd = other._read_fd;
                _write_buf = other._write_buf;
            }
            return *this;
        }

        void    addFds(const ResolvedAction &action) {
            _read_fd = action.cgi_fds.first;
            _write_fd = action.cgi_fds.second;
            _is_cgi = true;
        }

        bool            isCgi() const {return _is_cgi;}
        int             getWritefd() const {return _write_fd;}
        int             getReadfd() const {return _read_fd;}
        std::string     &getWriteBuf() { return _write_buf;}
};
