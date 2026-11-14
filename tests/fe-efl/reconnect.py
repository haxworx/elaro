"""Exercise reconnect timing, channel restoration, limits, timeout and cancellation."""
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time

mode = sys.argv[2]
with socket.socket() as listener, tempfile.TemporaryDirectory() as home:
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    listener.settimeout(0.2)
    stopped = threading.Event()
    accepted = []
    joined = []
    quits = []
    errors = []

    def serve():
        try:
            while not stopped.is_set():
                try:
                    peer, _ = listener.accept()
                except TimeoutError:
                    continue
                accepted.append(time.monotonic())
                with peer:
                    peer.settimeout(40)
                    if mode in ('limit', 'cancel', 'quit') or (mode == 'timeout' and len(accepted) > 1):
                        continue
                    if mode == 'timeout':
                        while peer.recv(4096):
                            pass
                        continue
                    with peer.makefile('rb') as stream:
                        current = set()
                        for line in stream:
                            line = line.decode().strip()
                            if line.startswith('CAP LS'):
                                peer.sendall(b':mock CAP tester LS :\r\n')
                            if line.startswith('USER '):
                                peer.sendall(b':mock 001 tester :Welcome\r\n'
                                             b':mock 005 tester CHANTYPES=# PREFIX=(ov)@+ :supported\r\n'
                                             b':mock 376 tester :End of MOTD\r\n')
                                if mode == 'disabled':
                                    break
                            if line.startswith('PING '):
                                peer.sendall(('PONG ' + line[5:] + '\r\n').encode())
                            if line.startswith('JOIN ') and line != 'JOIN :':
                                fields = line.split()
                                if '#saved' in fields[1].split(','):
                                    assert len(fields) == 3, line
                                    assert fields[2].split(',')[fields[1].split(',').index('#saved')] == 'secret', line
                                for channel in line.split()[1].split(','):
                                    current.add(channel)
                                    peer.sendall(f':tester!u@host JOIN :{channel}\r\n'.encode())
                                if current == {'#saved', '#later'} and len(accepted) == 1 and not mode.startswith('quit-'):
                                    joined.append(current)
                                    break
                            if line.startswith('QUIT'):
                                quits.append(line)
                                joined.append(current)
                                break
        except Exception as error:
            if not stopped.is_set():
                errors.append(error)

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    env = dict(os.environ, HOME=home, XDG_CONFIG_HOME=home, XDG_CACHE_HOME=home)
    try:
        subprocess.run([sys.argv[1], str(listener.getsockname()[1]), mode],
                       env=env, check=True, timeout=82)
    finally:
        stopped.set()
        thread.join(timeout=2)
    assert not errors, errors
    expected = 2 if mode == 'success' else 6 if mode in ('limit', 'timeout') else 1
    assert len(accepted) == expected, accepted
    for before, after in zip(accepted, accepted[1:]):
        assert after - before >= 4.8, accepted
    if mode == 'timeout':
        assert accepted[1] - accepted[0] >= 34.8, accepted
    if mode == 'success':
        assert joined == [{'#saved', '#later'}, {'#saved', '#later'}], joined

    if mode.startswith('quit-'):
        reason = 'Gone fishing — back soon' if mode == 'quit-message' else 'Leaving'
        assert quits == ['QUIT :' + reason], quits
