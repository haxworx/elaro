"""Exercise ban-list queries, exact mask removal, empty lists and errors."""
import os
import socket
import subprocess
import sys
import tempfile
import threading

with socket.socket() as listener, tempfile.TemporaryDirectory() as home:
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    listener.settimeout(10)
    observed, errors = [], []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(35)
                stream = peer.makefile('rb')
                queries = 0
                for raw in stream:
                    line = raw.decode().strip()
                    observed.append(line)
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    if line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n:mock 376 tester :End\r\n')
                    if line.startswith('JOIN #test'):
                        peer.sendall(b':tester!u@host JOIN :#test\r\n')
                    if line == 'MODE #test b':
                        queries += 1
                        if queries == 1:
                            peer.sendall(b':mock 367 tester #test *!*@bad.example setter 1794500000\r\n')
                        if queries == 3:
                            peer.sendall(b':mock 482 tester #test :Channel operator privileges needed\r\n')
                        else:
                            peer.sendall(b':mock 368 tester #test :End of channel ban list\r\n')
                    if line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve)
    worker.start()
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1])],
                            env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': home}, timeout=40)
    worker.join(timeout=5)
    assert result.returncode == 0, (result.returncode, observed, errors)
    assert not errors, errors
    assert observed.count('MODE #test b') == 3, observed
    assert observed.count('MODE #test -b *!*@bad.example') == 1, observed
