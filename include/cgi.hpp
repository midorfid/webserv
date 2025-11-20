#pragma once

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

        void    addFds(int read_fd, int write_fd) {
            _read_fd = read_fd;
            _write_fd = write_fd;
        }

        bool            isCgi() const {return _is_cgi;}
        int             getWritefd() const {return _write_fd;}
        int             getReadfd() const {return _read_fd;}
        std::string     &getWriteBuf() { return _write_buf;}
};
