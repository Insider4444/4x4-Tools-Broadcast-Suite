"""Exercise the exact Setup download function against an isolated local server."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import hashlib
import os
import re
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]


def main():
    data = os.urandom(1024 * 1024 + 31)
    requests = []
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass
        def do_GET(self):
            start = int(self.headers.get('Range', 'bytes=0-').split('=')[1].split('-')[0])
            requests.append(start)
            self.send_response(206 if start else 200)
            if start:
                self.send_header('Content-Range', f'bytes {start}-{len(data)-1}/{len(data)}')
            self.send_header('Content-Length', str(len(data)-start)); self.end_headers()
            # A graceful but premature close must leave a resumable partial,
            # rather than promoting a short payload as a completed download.
            self.wfile.write(data[start:len(data)//2] if len(requests) == 1 else data[start:])
            self.close_connection = True
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
    (ROOT / 'build').mkdir(exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix='download-fixture-', dir=ROOT / 'build') as temp:
            folder = Path(temp)
            source = (ROOT / 'installer/install-suite.ps1').read_text(encoding='utf-8-sig')
            function = re.search(r'(?ms)^function DownloadPayload\(.*?(?=^function WriteIni)', source).group()
            script = folder / 'test.ps1'
            script.write_text("$ErrorActionPreference='Stop'\n" + function +
                              f"\nDownloadPayload 'http://127.0.0.1:{server.server_port}/payload' (Join-Path $PSScriptRoot 'payload.bin') {len(data)}\n", encoding='utf-8-sig')
            result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)], cwd=ROOT, capture_output=True, text=True)
            if result.returncode:
                print(result.stdout, result.stderr); raise SystemExit(result.returncode)
            target = folder / 'payload.bin'
            if target.read_bytes() != data or (folder / 'payload.bin.download').exists() or requests != [0, len(data)//2]:
                raise RuntimeError('Download did not resume exactly after truncation')
            print('PASS: truncated HTTP transfer resumed with Range, exact final payload, no partial left.')
    finally:
        server.shutdown(); server.server_close()


if __name__ == '__main__':
    main()
