# ── Build stage ───────────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        g++ \
        make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/install && \
    cmake --build build --target install -- -j"$(nproc)"

# ── Runtime stage ─────────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/bin/webserv         /usr/local/bin/webserv
COPY --from=builder /install/etc/webserv         /etc/webserv
COPY --from=builder /install/var/www/html        /var/www/html

RUN mkdir -p /var/log/webserv /var/www/html/uploads

EXPOSE 8080

CMD ["/usr/local/bin/webserv", "/etc/webserv/webserv.conf"]
