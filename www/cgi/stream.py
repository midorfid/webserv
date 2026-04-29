#!/usr/bin/env python3
"""CGI chunked-streaming demo.
Deliberately omits Content-Length so the server must use Transfer-Encoding: chunked.
Each chunk is flushed individually with a 1-second pause between them."""

import sys
import time

CHUNKS = [
    ("<h2>Chunk 1 / 5</h2><p>Connection established. Stream started.</p>", 1),
    ("<h2>Chunk 2 / 5</h2><p>1 second has passed. Still streaming&hellip;</p>", 1),
    ("<h2>Chunk 3 / 5</h2><p>Halfway there. Server is holding the connection open.</p>", 1),
    ("<h2>Chunk 4 / 5</h2><p>Almost done. Each chunk is a separate TCP write.</p>", 1),
    ("<h2>Chunk 5 / 5</h2><p><strong>Stream complete!</strong> "
     "Your server handled Transfer-Encoding: chunked correctly.</p>", 0),
]

STYLE = """
<link rel="stylesheet" href="/style.css">
<style>
  body { background: #0d1117; }
  .chunk {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: #161b22;
    color: #c9d1d9;
    border-left: 4px solid #238636;
    padding: 1rem 1.5rem;
    margin: 1rem 2rem;
    border-radius: 0 8px 8px 0;
    animation: fadein 0.4s ease;
  }
  .chunk h2 { color: #7ee787; font-size: 1rem; margin-bottom: 0.3rem; }
  .chunk p  { color: #8b949e; margin: 0; font-size: 0.9rem; }
  @keyframes fadein { from { opacity:0; transform:translateY(8px); } to { opacity:1; } }
  #header { color: #e6edf3; font-family: 'Segoe UI', system-ui, sans-serif;
            padding: 2rem 2rem 0.5rem; font-size: 1.4rem; font-weight: 700; }
  #sub    { color: #8b949e; font-family: 'Segoe UI', system-ui, sans-serif;
            padding: 0 2rem 1.5rem; font-size: 0.88rem; }
</style>
"""

def main():
    # Headers only — NO Content-Length
    sys.stdout.write("Status: 200 OK\r\n")
    sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.flush()

    # Page scaffold
    sys.stdout.write(f"<!DOCTYPE html><html lang='en'><head>"
                     f"<meta charset='UTF-8'><title>Chunked Stream — webserv</title>"
                     f"{STYLE}</head><body>"
                     f"<div id='header'>&#128311; Chunked Transfer-Encoding Demo</div>"
                     f"<div id='sub'>Each block below arrives as a separate HTTP chunk, "
                     f"1 second apart. Watch them appear in real time.</div>")
    sys.stdout.flush()

    for content, delay in CHUNKS:
        sys.stdout.write(f"<div class='chunk'>{content}</div>")
        sys.stdout.flush()
        if delay:
            time.sleep(delay)

    sys.stdout.write("</body></html>")
    sys.stdout.flush()


if __name__ == "__main__":
    main()
