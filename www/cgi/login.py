#!/usr/bin/env python3
"""CGI login handler — reads POST body, verifies against users.json,
issues a session cookie, and redirects to profile.py on success."""

import sys
import os
import json
import secrets
import urllib.parse
import time

CGI_DIR      = os.path.dirname(os.path.abspath(__file__))
USERS_FILE   = os.path.join(CGI_DIR, "users.json")
SESSION_FILE = os.path.join(CGI_DIR, "sessions.json")


def load_json(path, default):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return default


def save_json(path, data):
    with open(path, "w") as f:
        json.dump(data, f, indent=2)


def redirect(location):
    sys.stdout.write(f"Status: 302 Found\r\n")
    sys.stdout.write(f"Location: {location}\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.flush()


def login_page(error_msg=""):
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Login — webserv</title>
  <link rel="stylesheet" href="/style.css">
  <style>main{{max-width:420px}} .card{{margin-top:2.5rem}}</style>
</head>
<body>
  <nav>
    <a href="/" class="brand">&#9670; webserv</a>
    <a href="/">Home</a><a href="/upload.html">Upload</a>
    <a href="/login.html" class="active">Login</a>
    <a href="/cgi/stream.py">Stream</a>
  </nav>
  <main>
    <div class="card">
      <h2>&#128274; Sign In</h2>
      {"<div class='alert alert-err'>" + error_msg + "</div>" if error_msg else ""}
      <form method="POST" action="/cgi/login.py">
        <div class="form-group">
          <label for="username">Username</label>
          <input type="text" id="username" name="username" required autofocus>
        </div>
        <div class="form-group">
          <label for="password">Password</label>
          <input type="password" id="password" name="password" required>
        </div>
        <button type="submit" class="btn" style="width:100%">Sign In</button>
      </form>
    </div>
  </main>
</body>
</html>"""
    body = html.encode("utf-8")
    sys.stdout.buffer.write(b"Status: 401 Unauthorized\r\n")
    sys.stdout.buffer.write(b"Content-Type: text/html; charset=utf-8\r\n")
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n".encode())
    sys.stdout.buffer.write(b"\r\n")
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()


def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    if method != "POST":
        redirect("/login.html")
        return

    # Read POST body
    try:
        length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
    except ValueError:
        length = 0
    raw = sys.stdin.buffer.read(length) if length > 0 else b""
    params = urllib.parse.parse_qs(raw.decode("utf-8", errors="replace"), keep_blank_values=True)

    username = params.get("username", [""])[0].strip()
    password = params.get("password", [""])[0]

    users = load_json(USERS_FILE, {})
    if not username or users.get(username) != password:
        login_page("Invalid username or password.")
        return

    # Create session
    session_id = secrets.token_hex(24)
    sessions   = load_json(SESSION_FILE, {})
    sessions[session_id] = {
        "username": username,
        "created":  int(time.time()),
    }
    save_json(SESSION_FILE, sessions)

    # Redirect to profile with cookie
    sys.stdout.write("Status: 302 Found\r\n")
    sys.stdout.write("Location: /cgi/profile.py\r\n")
    sys.stdout.write(f"Set-Cookie: session={session_id}; Path=/; HttpOnly\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
