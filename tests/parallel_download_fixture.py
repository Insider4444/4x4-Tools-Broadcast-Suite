"""Exercise the real three-worker download path, resume and streaming hashes."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    content = {name: os.urandom(1024 * 1024 + 31) for name in ['resume', 'restart', 'corrupt', 'plain-a', 'plain-b', 'complete']}
    calls = {name: [] for name in content}
    lock = threading.Lock()
    active = 0
    peak = 0
    barrier = threading.Barrier(2)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            nonlocal active, peak
            name = self.path.lstrip('/')
            data = content[name]
            requested = int(self.headers.get('Range', 'bytes=0-').split('=')[1].split('-')[0])
            with lock:
                calls[name].append(requested)
                first = len(calls[name]) == 1
                active += 1
                peak = max(peak, active)
            try:
                if name in ('resume', 'restart') and first:
                    barrier.wait(timeout=10)
                offset = 0 if name == 'restart' else requested
                self.send_response(206 if offset else 200)
                if offset:
                    self.send_header('Content-Range', f'bytes {offset}-{len(data)-1}/{len(data)}')
                self.send_header('Content-Length', str(len(data)-offset))
                self.end_headers()
                body = data[offset:]
                if name == 'resume' and first:
                    body = data[offset:len(data)//2]
                if name == 'corrupt' and first:
                    body = b'x' + body[1:]
                for start in range(0, len(body), 65536):
                    time.sleep(0.01)
                    self.wfile.write(body[start:start+65536])
                    self.wfile.flush()
                self.close_connection = True
            finally:
                with lock:
                    active -= 1

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    (ROOT / 'build').mkdir(exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix='parallel-download-', dir=ROOT / 'build') as temp:
            folder = Path(temp)
            items = []
            for name, data in content.items():
                target = folder / (name + '.bin')
                items.append(dict(url=f'http://127.0.0.1:{server.server_port}/{name}', target=str(target),
                                  bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), label=name))
                if name in ('resume', 'restart'):
                    Path(str(target) + '.download').write_bytes(data[:12345])
                if name == 'complete':
                    Path(str(target) + '.download').write_bytes(data)
            (folder / 'items.json').write_text(json.dumps(items), encoding='utf-8')
            source = (ROOT / 'installer/install-suite.ps1').read_text(encoding='utf-8-sig')
            guards = re.search(r'(?ms)^function Full\(.*?(?=^function Hash)', source).group()
            functions = re.search(r'(?ms)^function Hash\(.*?(?=^function WriteIni)', source).group()
            code = ("$ErrorActionPreference='Stop'\n" + guards + functions +
                    "\nAdd-Type -Path '" + str(ROOT / 'installer/setup-progress.cs').replace("'", "''") +
                    "' -ReferencedAssemblies ([Uri].Assembly.Location)\n" +
                    "[FourByFour.SetupProgress]::Reset()\n" +
                    "$downloadItems=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'items.json') -Raw | ConvertFrom-Json; DownloadBatch -Items $downloadItems\n" +
                    "if([FourByFour.SetupProgress]::Percent -lt 45){throw 'Download progress did not complete'}\n" +
                    "if([FourByFour.SetupProgress]::LastStatus -ne 'Downloads complete'){throw 'Final status missing'}\n")
            script = folder / 'run.ps1'
            script.write_text(code, encoding='utf-8-sig')
            result = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)],
                                    cwd=ROOT, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            for name, data in content.items():
                assert (folder / (name + '.bin')).read_bytes() == data
                assert not (folder / (name + '.bin.download')).exists()
            assert 2 <= peak <= 3, (peak, calls)
            assert calls['resume'] == [12345, len(content['resume'])//2], calls
            assert calls['restart'] == [12345], calls
            assert calls['corrupt'] == [0, 0], calls
            assert calls['complete'] == [], calls
    finally:
        server.shutdown()
        server.server_close()
    print('PASS: three-worker overlap, exact Range resume, ignored-Range restart, corrupt transfer retry, complete-partial reuse and live progress.')


if __name__ == '__main__':
    main()
