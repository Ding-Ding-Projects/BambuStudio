"""Bounded static-file server for an owned off-screen verification lane."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
import mimetypes
import sys

root = Path(sys.argv[1]).resolve(strict=True)
port = int(sys.argv[2])
if not 1024 <= port <= 65535:
    raise ValueError("Invalid port")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        try:
            name = unquote(urlsplit(self.path).path).lstrip("/") or "index.html"
            target = (root / name).resolve(strict=True)
            if not target.is_relative_to(root) or not target.is_file() or target.stat().st_size > 16 * 1024 * 1024:
                raise ValueError("Resource rejected")
            data = target.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", mimetypes.guess_type(target.name)[0] or "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(data)
        except (ValueError, OSError):
            self.send_response(404)
            self.end_headers()


ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
