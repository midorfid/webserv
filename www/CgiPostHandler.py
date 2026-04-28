#!/usr/bin/python3
import os
import sys
import urllib.parse
import logCgiExec

UPLOAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uploads")

class CGIHandler:
    def __init__(self):
        self.mime_type = os.environ.get("MIME_TYPE", "")
        self.cont_len = int(os.environ.get("CONTENT_LENGTH", '0'))
        self.method = os.environ.get("REQUEST_METHOD", "GET")

    def sendResponse(self, status="200 OK", body=""):
        body_bytes = body.encode("utf-8")
        sys.stdout.write(f"Status: {status}\r\n")
        sys.stdout.write("Content-Type: text/plain\r\n")
        sys.stdout.write(f"Content-Length: {len(body_bytes)}\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.buffer.write(body_bytes)
        sys.stdout.flush()

    def handle_bin_stream(self, filename="upload.bin"):
        try:
            os.makedirs(UPLOAD_DIR, exist_ok=True)
            path = os.path.join(UPLOAD_DIR, os.path.basename(filename))
            bytes_remaining = self.cont_len
            with open(path, "wb") as f:
                while bytes_remaining > 0:
                    chunk = sys.stdin.buffer.read(min(bytes_remaining, 4096))
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

    def handle_multipart(self):
        # Extract boundary from the full Content-Type value (MIME_TYPE strips parameters)
        full_ct = os.environ.get("CONTENT_TYPE", "")
        boundary = None
        for param in full_ct.split(";"):
            param = param.strip()
            if param.lower().startswith("boundary="):
                boundary = param[len("boundary="):].strip().strip('"')
                break
        if not boundary:
            return self.sendResponse("400 Bad Request", "Missing multipart boundary")

        body = sys.stdin.buffer.read(self.cont_len)
        delimiter = b"--" + boundary.encode()

        parts = body.split(delimiter)
        fields = {}
        saved_files = []

        for part in parts[1:]:             # first element is preamble (empty)
            if part.lstrip(b"\r\n").startswith(b"--"):  # final --boundary--
                break
            # Strip leading \r\n added by the boundary line
            if part.startswith(b"\r\n"):
                part = part[2:]
            if part.endswith(b"\r\n"):
                part = part[:-2]

            header_end = part.find(b"\r\n\r\n")
            if header_end == -1:
                continue
            headers_raw = part[:header_end].decode("utf-8", errors="replace")
            part_body   = part[header_end + 4:]

            # Parse part headers into a dict
            part_headers = {}
            for line in headers_raw.split("\r\n"):
                if ":" in line:
                    k, v = line.split(":", 1)
                    part_headers[k.strip().lower()] = v.strip()

            # Parse Content-Disposition parameters
            disposition = part_headers.get("content-disposition", "")
            disp_params = {}
            for item in disposition.split(";"):
                item = item.strip()
                if "=" in item:
                    k, v = item.split("=", 1)
                    disp_params[k.strip().lower()] = v.strip().strip('"')

            name     = disp_params.get("name", "")
            filename = disp_params.get("filename", "")

            if filename:
                try:
                    os.makedirs(UPLOAD_DIR, exist_ok=True)
                    save_path = os.path.join(UPLOAD_DIR, os.path.basename(filename))
                    with open(save_path, "wb") as f:
                        f.write(part_body)
                    saved_files.append(f"'{filename}' ({len(part_body)} bytes) → field '{name}'")
                except Exception as e:
                    return self.sendResponse("500 Internal Server Error",
                                             f"Failed to save '{filename}': {e}")
            else:
                fields[name] = part_body.decode("utf-8", errors="replace")

        lines = []
        if fields:
            lines.append(f"Fields: { {k: v for k, v in fields.items()} }")
        for info in saved_files:
            lines.append(f"Saved: {info}")
        if not lines:
            lines.append("No parts found")
        return self.sendResponse("200 OK", "\n".join(lines))

    @logCgiExec.LogTime
    def process(self):
        if self.method != "POST":
            return self.sendResponse("405 Method Not Allowed", "Use POST")
        if self.mime_type == "application/octet-stream":
            if self.handle_bin_stream():
                return self.sendResponse("201 Created", "Binary File Stored")
            else:
                return self.sendResponse("500 Internal Server Error", "Disk Write Failed")
        elif self.mime_type == "application/x-www-form-urlencoded":
            data = self.handle_form_data()
            return self.sendResponse("200 OK", f"Parsed Keys: {list(data.keys())}")
        elif self.mime_type == "text/plain":
            text = sys.stdin.buffer.read(self.cont_len).decode("utf-8", errors="replace")
            return self.sendResponse("200 OK", f"Received:\n{text}")
        elif self.mime_type == "multipart/form-data":
            return self.handle_multipart()
        else:
            return self.sendResponse("415 Unsupported Media Type",
                                     f"Unsupported MIME type: {self.mime_type}")


if __name__ == "__main__":
    handler = CGIHandler()
    handler.process()