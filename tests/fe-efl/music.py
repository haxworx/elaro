"""Exercise the EFL playback protocol with local, silent audio."""
import http.server
import io
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
import wave

buffer = io.BytesIO()
with wave.open(buffer, 'wb') as audio:
    audio.setnchannels(1)
    audio.setsampwidth(2)
    audio.setframerate(22050)
    audio.writeframes(b'\0\0' * 22050 * 5)
content = buffer.getvalue()
requests = []


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        requests.append(self.path)
        # HTTPS/remote streams can preroll for more than a second before playback starts.
        time.sleep(2)
        self.send_response(200)
        self.send_header('Content-Type', 'audio/wav')
        self.send_header('Content-Length', str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *args):
        pass


with tempfile.TemporaryDirectory() as home:
    env = {**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config'), 'ELM_ENGINE': 'buffer',
           'ECORE_NO_SYSTEM_MODULES': '1', 'EINA_LOG_LEVEL': '0'}
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    process = subprocess.Popen([sys.argv[1], '--music-backend'], env=env,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, text=True)
    lines = queue.Queue()

    def read_lines():
        for line in process.stdout:
            lines.put(line.strip())
        lines.put('EXIT')

    threading.Thread(target=read_lines, daemon=True).start()

    def send(command):
        process.stdin.write(command + '\n')
        process.stdin.flush()

    def wait_state(predicate):
        deadline = time.monotonic() + 10
        observed = []
        while time.monotonic() < deadline:
            line = lines.get(timeout=max(0.01, deadline - time.monotonic()))
            observed.append(line)
            assert line not in ('ERROR', 'EXIT'), observed
            fields = line.split()
            if len(fields) == 6 and fields[0] == 'STATE':
                state = (float(fields[1]), float(fields[2]), *(int(x) for x in fields[3:]))
                if predicate(state):
                    return state
        raise AssertionError(observed)

    try:
        url = f'http://127.0.0.1:{server.server_port}/silence.wav?x=1&y=2'
        started = time.monotonic()
        send('OPEN ' + urllib.parse.quote(url, safe=''))
        send('VOLUME 0')
        wait_state(lambda s: s[4] and s[2] and s[0] > 0)
        assert time.monotonic() - started >= 2
        assert requests == ['/silence.wav?x=1&y=2'], requests
        send('PAUSE')
        wait_state(lambda s: s[4] and not s[2])
        send('SEEK 2')
        wait_state(lambda s: not s[2] and abs(s[0] - 2) < 0.2)
        send('PLAY')
        wait_state(lambda s: s[2] and s[0] > 2.2)
        send('STOP')
        wait_state(lambda s: not s[2] and s[0] < 0.2)
        process.stdin.close()
        assert process.wait(timeout=8) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        server.shutdown()
        server.server_close()

    # The helper rejects local-file URLs and quits cleanly when its owner closes stdin.
    rejected = subprocess.run([sys.argv[1], '--music-backend'], env=env,
                              input='OPEN file%3A%2F%2F%2Ftmp%2Fprivate.mp3\n',
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              text=True, timeout=10)
    assert rejected.returncode == 0
    assert 'ERROR' in rejected.stdout.splitlines()
