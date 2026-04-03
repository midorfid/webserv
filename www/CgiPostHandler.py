#!/usr/bin/python3
import os
import sys
import urllib.parse
import logCgiExec

class CGIHandler:
    def __init__(self):
        self.mime_type = os.environ.get("CONTENT_TYPE", "")
        self.cont_len = int(os.environ.get("CONTENT_LENGTH", '0'))
        self.method = os.environ.get("METHOD", "GET")

    def sendResponse(self, status = "200 OK", body = ""):
        sys.stdout.write(f"Status: {status}\r\n")
        sys.stdout.write("Content-Type: text/plain\r\n")
        sys.stdout.write(f"Content-Len: {len(body)}\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.write(body)
        sys.stdout.flush

    def handle_bin_stream(self, filename = "upload.bin"):
        try:
            bytes_remaining = self.cont_len

            with open(filename, "wb") as f:
                while bytes_remaining > 0:
                    chunk_size = min(bytes_remaining, 4096)

                    chunk = sys.stdin.buffer.read(chunk_size)

                    if not chunk:
                        break

                    f.write(chunk)

                    bytes_remaining -= len(chunk)
                return True
        except Exception as e:
            print(f"Error occurred: {e}", file=sys.stderr)
            return False
        
    def handle_form_data(self):
        return urllib.parse.parse_qs(sys.stdin.buffer.read(self.cont_len).decode())

    @logCgiExec.LogTime
    def process(self):
        if self.method != "POST":
            return self.sendResponse("405 Method Not Allowed", "Use POST")    
        if self.mime_type == "application/octet-stream":
            if self.handle_bin_stream():
                return self.sendResponse("200 OK", "Binary File Stored")
            else:
                return self.sendResponse("500 Internal Error", "Disk Write Failed")
        elif self.mime_type == "application/x-www-form-urlencoded":
            data = self.handle_form_data()
            return self.sendResponse("200 OK", f"Parsed Keys: {list(data.keys())}")
        else:
            self.sendResponse("415 Unsupported Media Type")


if __name__ == "__main__":
    print("HTTP/1.1 201 OK", end="\r\n")
    handler = CGIHandler()
    handler.process()