"""Verify the frontend dispatches standard /mode syntax through Irssi."""
import os
import socket
import subprocess
import sys
import tempfile
import threading

expected = [
    'MODE #test +nt', 'MODE #test -m', 'MODE #test +l 50',
    'MODE #test +k secret', 'MODE #test +ov alice bob',
    'MODE #test', 'MODE #test', 'MODE #elsewhere +i',
]
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
                for raw in stream:
                    line = raw.decode().strip()
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :\r\n')
                    if line.startswith('USER '):
                        peer.sendall(b':mock 001 tester :Welcome\r\n'
                                     b':mock 005 tester CHANMODES=beI,k,l,imnpst PREFIX=(ov)@+ :supported\r\n'
                                     b':mock 376 tester :End\r\n')
                    if line.startswith('JOIN #test'):
                        peer.sendall(b':tester!u@host JOIN :#test\r\n')
                    if line.startswith('MODE #'):
                        observed.append(line)
                        peer.sendall(b':mock NOTICE tester :mode received\r\n')
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
    assert observed == expected, observed
