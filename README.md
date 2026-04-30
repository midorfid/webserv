# webserv

### A high-performance HTTP/1.1 server built from scratch in C++17

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey?logo=linux)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)

---

![Demo](TODO)

---

## Overview

**webserv** is a fully hand-written HTTP/1.1 server implemented in C++17 using raw Linux POSIX APIs — no Boost, no libuv, no framework. It was built as a deep-dive into systems programming: the goal was to understand exactly what happens between a client sending bytes and a server writing a response, at every layer of the stack.

The server implements a non-blocking `epoll`-driven event loop with a per-client state machine, true asynchronous CGI execution over non-blocking pipes, chunked transfer encoding for streaming responses, and a custom configuration file parser. Every subsystem — from socket binding to CGI environment construction to session ID generation — is written from first principles.

---

## Features

- **Non-blocking `epoll` event loop** — single-threaded, level-triggered I/O; `EPOLLIN` and `EPOLLOUT` paths are fully separated; `EPOLLRDHUP`/`EPOLLHUP`/`EPOLLERR` handled explicitly
- **Client state machine** — each connection progresses through `IDLE → READING_HEADERS → READING_BODY → WRITING_RESPONSE → DONE`; keep-alive connections reset and cycle back to `IDLE`
- **Asynchronous CGI execution** — `fork()` + `pipe()` + `execve()`; CGI stdin/stdout mapped to non-blocking pipe FDs registered with epoll; orphaned processes killed via `SIGKILL` on timeout
- **Chunked Transfer-Encoding** — CGI output without a `Content-Length` is streamed to the client as HTTP/1.1 chunked frames in real time; terminal `0\r\n\r\n` sent on pipe EOF
- **`sendfile(2)` for static files** — zero-copy kernel path for all static file serving; no userspace buffer copy
- **Path traversal protection** — `realpath()` resolves symlinks on every request path and rejects any result that escapes the configured document root with `403 Forbidden`
- **Virtual hosting** — multiple `server {}` blocks on the same port; request routed by `Host` header to the correct vhost
- **Session management** — server-side sessions backed by `/dev/urandom` (128-bit IDs); sliding 30-minute TTL; `HttpOnly; SameSite=Lax` cookies injected automatically into all response types
- **Custom config parser** — nginx-inspired syntax; `location` blocks, `limit_except` method bitmasks, `autoindex`, `error_page`, `return` redirects, `keepalive_timeout`, `client_max_body_size`
- **HTTP compliance** — `100 Continue` for large uploads, duplicate security-sensitive header detection (`400`), HTTP version validation (`505`), correct MIME types on all responses
- **Docker support** — multi-stage build produces a minimal Debian runtime image; FHS-compliant paths

---

## Architecture

### Non-blocking event loop

The server runs a single `epoll_wait` loop. Each iteration dispatches file descriptor events by type — client reads, client writes, and CGI pipe I/O are all handled as separate, non-blocking code paths. No call ever blocks the loop.

```
epoll_wait()
  ├── listen fd      → accept(), register new client fd
  ├── client fd (IN) → recv() into request buffer, advance state machine
  ├── client fd (OUT)→ send() from response queue / sendfile() file body
  ├── CGI read fd    → read() CGI stdout, encode as chunked frames, wake client
  └── CGI write fd   → write() request body to CGI stdin, close when done
```

### Client state machine

```
IDLE ──recv()──► READING_HEADERS ──headers complete──► READING_BODY
                                                              │
                                                     body complete
                                                              │
                                                              ▼
                                                    WRITING_RESPONSE ──done──► IDLE (keep-alive)
                                                                             └──► DONE (close)
```

### CGI multiplexing

When a request targets a CGI script, `resolveCgiScript` forks a child process with `stdin`/`stdout` redirected to non-blocking pipes. Both pipe FDs are registered with epoll and mapped to the parent client via `_cgi_client`. The client socket is disarmed while CGI output accumulates; once CGI stdout closes (pipe EOF), the response is assembled and the client is re-armed for writing. A 30-second watchdog in `checkTimeouts()` kills hung scripts with `SIGKILL` and responds `504 Gateway Timeout`.

---

## Getting Started

### Prerequisites

| Tool | Version |
|------|---------|
| Linux | kernel ≥ 4.5 (epoll, sendfile) |
| g++ | ≥ 9 (C++17) |
| CMake | ≥ 3.10 |
| Python 3 | for CGI scripts |

### Build

```sh
git clone https://github.com/midorfid/webserv.git
cd webserv
cmake -S . -B build
make -C build -j$(nproc)
```

### Run

```sh
./build/webserv config/webserv.conf
```

### Docker

```sh
docker build -t webserv .
docker run -p 8080:8080 webserv
```

---

## Configuration

The server uses an nginx-inspired configuration syntax. A minimal example:

```nginx
server {
    listen              8080;
    server_names        example.org www.example.org;
    root                /var/www/html;
    index               index.html;
    client_max_body_size 10000000;
    keepalive_timeout   15;

    error_page 404      /error_pages/404.html;

    location / {
        limit_except GET POST {
            deny all;
        }
    }

    location /uploads {
        autoindex   on;
    }

    location /cgi {
        allow_cgi   .py;
    }

    location /old {
        return 301  /;
    }
}
```

Multiple `server {}` blocks on different ports or with different `server_names` enable full virtual hosting.

---

## Testing

Start the server and use the following commands to exercise the main code paths.

**Static file (GET)**
```sh
curl -i http://localhost:8080/index.html
```

**File upload (PUT)**
```sh
curl -i -X PUT --data-binary @photo.jpg http://localhost:8080/uploads/photo.jpg
```

**POST to a directory (auto-named file)**
```sh
curl -i -X POST -H "Content-Type: text/plain" --data "hello world" http://localhost:8080/uploads/
```

**CGI script (chunked streaming response)**
```sh
curl -i --no-buffer http://localhost:8080/cgi/stream.py
```

**DELETE**
```sh
curl -i -X DELETE http://localhost:8080/uploads/photo.jpg
```

**Verify path traversal is blocked**
```sh
curl -i http://localhost:8080/uploads/../../../../etc/passwd
# Expected: 403 Forbidden
```

**Inspect session cookie**
```sh
curl -i -c cookies.txt http://localhost:8080/
grep session_id cookies.txt
```

---

## Project Structure

```
webserv/
├── config/
│   ├── webserv.conf          # development config
│   └── webserv.docker.conf   # production (FHS paths)
├── include/                  # all headers
├── src/
│   ├── server.cpp            # epoll event loop, CGI multiplexing
│   ├── Client.cpp            # state machine, sendfile streaming
│   ├── RouteRequest.cpp      # request routing, path resolution
│   ├── RequestHandler.cpp    # response generation
│   ├── ParseRequest.cpp      # HTTP request parser
│   ├── ParseConfig.cpp       # config file parser
│   ├── response.cpp          # response builder, MIME types
│   ├── Session.cpp           # session store
│   └── Environment.cpp       # CGI environment builder
├── www/                      # document root (dev)
│   └── cgi/                  # Python CGI scripts
├── Dockerfile
└── CMakeLists.txt
```

---

## License

MIT
