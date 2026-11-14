"""Exercise native DCC receipt, consent, collisions, rejection and interruption."""
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time

with tempfile.TemporaryDirectory() as home:
    listeners = []
    for _ in range(5):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen()
        listener.settimeout(18)
        listeners.append(listener)
    irc, good, reject, broken, cancel = listeners
    errors = []
    offered_at = []
    payload = b'elaro DCC test\n' * 8192

    def send_file(listener, contents, cancellable=False):
        try:
            with listener.accept()[0] as peer:
                assert time.monotonic() - offered_at[0] >= 0.3, 'File accepted without consent'
                peer.settimeout(10)
                for offset in range(0, len(contents), 4096):
                    peer.sendall(contents[offset:offset + 4096])
                    time.sleep(0.04)
                peer.shutdown(socket.SHUT_WR)
                while peer.recv(4096):
                    pass
        except (BrokenPipeError, ConnectionResetError) as error:
            if not cancellable:
                errors.append(error)
        except Exception as error:
            errors.append(error)

    def serve_irc():
        try:
            with irc.accept()[0] as peer:
                stream = peer.makefile('rb')
                for raw in stream:
                    line = raw.decode().strip()
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    if line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n:mock 376 tester :End of MOTD\r\n')
                        offered_at.append(time.monotonic())
                        for listener, name, size in [(good, 'good.bin', len(payload)), (reject, 'reject.bin', 100), (broken, 'broken.bin', 100), (cancel, 'cancel.bin', 1048576)]:
                            peer.sendall(f':alice!u@host PRIVMSG tester :\x01DCC SEND {name} 2130706433 {listener.getsockname()[1]} {size}\x01\r\n'.encode())
                    if line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    threads = [threading.Thread(target=serve_irc), threading.Thread(target=send_file, args=(good, payload)), threading.Thread(target=send_file, args=(broken, b'partial')), threading.Thread(target=send_file, args=(cancel, b'x' * 1048576, True))]
    for thread in threads:
        thread.start()
    existing = Path(home) / 'existing'
    existing.write_bytes(b'preserve me')
    output = Path(home) / 'received'
    partial = Path(home) / 'partial'
    result = subprocess.run([sys.argv[1], str(irc.getsockname()[1]), str(existing), str(output), str(partial), str(Path(home) / 'cancelled')], env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config')}, timeout=20)
    for thread in threads:
        thread.join(timeout=3)
    reject.settimeout(0.1)
    try:
        connection, _ = reject.accept()
        connection.close()
        raise AssertionError('Rejected transfer connected')
    except TimeoutError:
        pass
    for listener in listeners:
        listener.close()
    assert result.returncode == 0, result.returncode
    assert not errors, errors
    assert existing.read_bytes() == b'preserve me'
    assert output.read_bytes() == payload
    assert partial.read_bytes() == b'partial'
    assert 0 < (Path(home) / 'cancelled').stat().st_size < 1048576
