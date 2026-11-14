"""Check a channel's Elementary input delivers the quit reason to the wire."""
import os
import socket
import subprocess
import sys
import tempfile
import threading

with tempfile.TemporaryDirectory() as home, socket.socket() as listener:
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    listener.settimeout(12)
    observed, errors = [], []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(12)
                for raw in peer.makefile('rb'):
                    line = raw.decode().rstrip('\r\n')
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    elif line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n'
                                     b':mock 005 tester CHANTYPES=# PREFIX=(ov)@+ :supported\r\n'
                                     b':mock 376 tester :End of MOTD\r\n')
                    elif line == 'JOIN #test':
                        peer.sendall(b':tester!u@host JOIN :#test\r\n')
                    elif line.startswith('QUIT '):
                        observed.append(line)
                        break
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve)
    worker.start()
    message = '/quit Gone fishing — back soon <&>'
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1]), message],
                            env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': home}, timeout=15)
    worker.join(timeout=12)
    assert result.returncode == 0, (result.returncode, observed, errors)
    assert not errors, errors
    assert observed == ['QUIT :Gone fishing — back soon <&>'], observed
