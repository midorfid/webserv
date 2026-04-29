#!/usr/bin/env python3
"""CGI logout — deletes the server-side session and clears the cookie."""

import sys
import os
import json

CGI_DIR      = os.path.dirname(os.path.abspath(__file__))
SESSION_FILE = os.path.join(CGI_DIR, "sessions.json")


def parse_cookies(header):
    cookies = {}
    for part in header.split(";"):
        if "=" in part:
            k, _, v = part.strip().partition("=")
            cookies[k.strip()] = v.strip()
    return cookies


def main():
    cookie_header = os.environ.get("HTTP_COOKIE", "")
    cookies       = parse_cookies(cookie_header)
    session_id    = cookies.get("session", "")

    if session_id:
        try:
            with open(SESSION_FILE) as f:
                sessions = json.load(f)
            sessions.pop(session_id, None)
            with open(SESSION_FILE, "w") as f:
                json.dump(sessions, f, indent=2)
        except (OSError, json.JSONDecodeError):
            pass

    # Expire the cookie and redirect home
    sys.stdout.write("Status: 302 Found\r\n")
    sys.stdout.write("Location: /login.html\r\n")
    sys.stdout.write("Set-Cookie: session=deleted; Path=/; HttpOnly; Max-Age=0\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
