"""Receive the exact file offered by the original DCC send engine."""
import os
from pathlib import Path
import shlex
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

with socket.socket() as listener, tempfile.TemporaryDirectory() as home:
    listener.bind(('127.0.0.1', 0)); listener.listen(); listener.settimeout(18)
    payload = b'outgoing elaro test\n' * 32768
    source = Path(home) / 'send [one] ?*.bin'
    source.write_bytes(payload)
    # A mistaken glob expansion must not send this neighbouring file.
    (Path(home) / 'send o xx.bin').write_bytes(b'wrong file')
    cancelled = Path(home) / 'cancel.bin'; cancelled.write_bytes(b'cancel me')
    received = bytearray()
    errors = []
    workers = []

    def receive(ip, port, size):
        try:
            with socket.create_connection((ip, port), timeout=10) as peer:
                while len(received) < size:
                    chunk = peer.recv(min(4096, size - len(received)))
                    assert chunk, 'Premature EOF'
                    received.extend(chunk)
                    peer.sendall(struct.pack('!I', len(received)))
                    time.sleep(0.005)
                # Allow the sender's EOF read to run before the final ACK.
                time.sleep(0.1)
                peer.sendall(struct.pack('!I', len(received)))
                time.sleep(0.1)
        except Exception as error:
            errors.append(error)

    def serve():
        try:
            with listener.accept()[0] as peer:
                for raw in peer.makefile('rb'):
                    line = raw.decode().strip()
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    if line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n:mock 376 tester :End\r\n:mock NOTICE tester :Ready to send\r\n')
                    if line.startswith('PRIVMSG alice :\x01DCC SEND '):
                        fields = shlex.split(line.split('\x01')[1])
                        name, address, port, size = fields[2:6]
                        if name == 'cancel.bin':
                            continue
                        assert name == source.name, fields
                        assert int(size) == len(payload), fields
                        ip = socket.inet_ntoa(struct.pack('!I', int(address)))
                        worker = threading.Thread(target=receive, args=(ip, int(port), int(size)))
                        workers.append(worker); worker.start()
                    if line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve); worker.start()
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1]), str(source), str(cancelled)], env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config')}, timeout=20)
    worker.join(timeout=3)
    for receiver in workers:
        receiver.join(timeout=3)
    assert result.returncode == 0, (result.returncode, errors)
    assert not errors, errors
    assert received == payload
    assert source.read_bytes() == payload
