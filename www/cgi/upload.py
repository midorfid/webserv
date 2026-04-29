#!/usr/bin/env python3
"""CGI file upload handler — parses multipart/form-data and saves the file to /uploads/."""

import sys
import os
import cgi
import html

WWW_DIR     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPLOADS_DIR = os.path.join(WWW_DIR, "uploads")


def respond(status, body_html):
    body = body_html.encode("utf-8")
    sys.stdout.buffer.write(f"Status: {status}\r\n".encode())
    sys.stdout.buffer.write(b"Content-Type: text/html; charset=utf-8\r\n")
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n".encode())
    sys.stdout.buffer.write(b"\r\n")
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()


def page(alert_class, message, filename=""):
    detail = f"<p>Saved as <code>/uploads/{html.escape(filename)}</code></p>" if filename else ""
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Upload Result — webserv</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <nav>
    <a href="/" class="brand">&#9670; webserv</a>
    <a href="/">Home</a>
    <a href="/upload.html" class="active">Upload</a>
    <a href="/login.html">Login</a>
    <a href="/cgi/stream.py">Stream</a>
  </nav>
  <main>
    <div class="card">
      <h2>Upload Result</h2>
      <div class="alert {alert_class}">{html.escape(message)}</div>
      {detail}
      <a class="btn btn-outline" href="/upload.html">Upload another</a>
      &nbsp;
      <a class="btn btn-outline" href="/uploads">Browse /uploads</a>
    </div>
  </main>
</body>
</html>"""


def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    if method != "POST":
        respond("405 Method Not Allowed", page("alert-err", "Only POST is supported."))
        return

    os.makedirs(UPLOADS_DIR, exist_ok=True)

    # cgi.FieldStorage reads CONTENT_TYPE and CONTENT_LENGTH from env automatically
    form = cgi.FieldStorage()
    file_item = form["file"] if "file" in form else None

    if not file_item or not file_item.filename:
        respond("400 Bad Request", page("alert-err", "No file was included in the request."))
        return

    safe_name = os.path.basename(file_item.filename).replace(" ", "_")
    if not safe_name:
        respond("400 Bad Request", page("alert-err", "Invalid filename."))
        return

    dest = os.path.join(UPLOADS_DIR, safe_name)
    with open(dest, "wb") as f:
        f.write(file_item.file.read())

    size = os.path.getsize(dest)
    respond("200 OK", page("alert-ok", f"Successfully uploaded '{safe_name}' ({size} bytes).", safe_name))


if __name__ == "__main__":
    main()
