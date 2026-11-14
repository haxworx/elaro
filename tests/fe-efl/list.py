"""Exercise large unsorted LIST results, joining, empty/error lists and cancellation."""
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

with socket.socket() as listener, tempfile.TemporaryDirectory() as home:
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    listener.settimeout(10)
    observed, errors = [], []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(40)
                stream = peer.makefile('rb')
                lists = 0
                for raw in stream:
                    line = raw.decode().strip()
                    observed.append(line)
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    if line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n'
                                     b':mock 376 tester :End\r\n')
                    if line == 'LIST':
                        lists += 1
                        peer.sendall(b':mock 321 tester Channel :Users Name\r\n')
                        if lists in (1, 4, 5):
                            rows = range(4999, -1, -1) if lists == 1 else [0]
                            payload = ''.join(f':mock 322 tester #{"room" if n % 2 else "Room"}{n} {n} :Topic <tag> & text\r\n' for n in rows)
                            if lists == 1:
                                halfway = len(payload) // 2
                                boundary = payload.index('\r\n', halfway) + 2
                                peer.sendall(payload[:boundary].encode())
                                peer.sendall(b':alice!u@host PRIVMSG tester :hello during list\r\n')
                                peer.sendall(payload[boundary:].encode())
                            else:
                                peer.sendall(payload.encode())
                        if lists == 4:
                            peer.sendall(b':mock NOTICE tester :cancel-list\r\n')
                            time.sleep(0.1)
                            peer.sendall(b':mock 323 tester :End of cancelled list\r\n')
                        elif lists == 3:
                            peer.sendall(b':mock 263 tester LIST :Please try again later\r\n')
                        else:
                            peer.sendall(b':mock 323 tester :End of list\r\n')
                    if line == 'JOIN #Room2':
                        peer.sendall(b':tester!u@host JOIN :#Room2\r\n')
                    if line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve)
    worker.start()
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1])],
                            env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': home}, timeout=45)
    worker.join(timeout=5)
    assert result.returncode == 0, (result.returncode, observed, errors)
    assert not errors, errors
    assert observed.count('LIST') == 5, observed
    assert 'JOIN #Room2' in observed, observed
