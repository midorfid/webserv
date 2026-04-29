#!/usr/bin/env python3
"""CGI profile page — validates session cookie and shows a personalised greeting."""

import sys
import os
import json
import time

CGI_DIR      = os.path.dirname(os.path.abspath(__file__))
SESSION_FILE = os.path.join(CGI_DIR, "sessions.json")

SESSION_MAX_AGE = 3600  # seconds


def load_json(path, default):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return default


def parse_cookies(cookie_header):
    cookies = {}
    for part in cookie_header.split(";"):
        if "=" in part:
            k, _, v = part.strip().partition("=")
            cookies[k.strip()] = v.strip()
    return cookies


def respond(status, body_html, extra_headers=""):
    body = body_html.encode("utf-8")
    sys.stdout.buffer.write(f"Status: {status}\r\n".encode())
    sys.stdout.buffer.write(b"Content-Type: text/html; charset=utf-8\r\n")
    if extra_headers:
        sys.stdout.buffer.write(extra_headers.encode())
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n".encode())
    sys.stdout.buffer.write(b"\r\n")
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()


def page(title, content, nav_extra=""):
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{title} — webserv</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <nav>
    <a href="/" class="brand">&#9670; webserv</a>
    <a href="/">Home</a><a href="/upload.html">Upload</a>
    <a href="/login.html">Login</a>
    <a href="/cgi/stream.py">Stream</a>
    {nav_extra}
  </nav>
  <main>{content}</main>
</body>
</html>"""


def main():
    cookie_header = os.environ.get("HTTP_COOKIE", "")
    cookies       = parse_cookies(cookie_header)
    session_id    = cookies.get("session", "")

    sessions = load_json(SESSION_FILE, {})
    session  = sessions.get(session_id)

    # Validate session exists and is not expired
    if not session or (int(time.time()) - session.get("created", 0)) > SESSION_MAX_AGE:
        body = page("Access Denied",
            """<div class="card">
                 <h2>&#128683; Access Denied</h2>
                 <div class="alert alert-err">No valid session found. Please log in first.</div>
                 <a class="btn" href="/login.html">Go to Login</a>
               </div>""")
        respond("403 Forbidden", body)
        return

    username = session["username"]
    created  = session["created"]
    age_min  = (int(time.time()) - created) // 60

    content = f"""
      <div class="hero">
        <h1>Welcome back, {username}!</h1>
        <p>Your session is active. Cookie verified server-side.</p>
      </div>
      <div class="card">
        <h2>Session Details</h2>
        <table style="border-collapse:collapse;width:100%;font-size:0.9rem">
          <tr><td style="padding:0.5rem 0;color:var(--muted);width:160px">Username</td>
              <td><strong>{username}</strong></td></tr>
          <tr><td style="padding:0.5rem 0;color:var(--muted)">Session ID</td>
              <td><code style="font-size:0.8rem">{session_id[:16]}…</code></td></tr>
          <tr><td style="padding:0.5rem 0;color:var(--muted)">Session age</td>
              <td>{age_min} minute(s)</td></tr>
          <tr><td style="padding:0.5rem 0;color:var(--muted)">Expires in</td>
              <td>{60 - age_min} minute(s)</td></tr>
        </table>
      </div>
      <div class="card">
        <a class="btn btn-outline" href="/cgi/logout.py">Sign Out</a>
      </div>"""

    respond("200 OK", page(f"Profile — {username}", content))


if __name__ == "__main__":
    main()
