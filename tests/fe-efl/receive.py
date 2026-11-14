"""Both clients must drain a TLS record even when the server then goes idle."""
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

with tempfile.TemporaryDirectory() as home, socket.socket() as listener:
    certificate = os.path.join(home, 'cert.pem')
    key = os.path.join(home, 'key.pem')
    subprocess.run([sys.argv[2], 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', key, '-out', certificate, '-days', '1',
                    '-subj', '/CN=127.0.0.1'], check=True, capture_output=True)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    listener.settimeout(12)
    errors = []

    def serve():
        try:
            with listener.accept()[0] as raw, context.wrap_socket(raw, server_side=True) as peer:
                peer.settimeout(12)
                stream = peer.makefile('rb')
                nick = None
                for line in stream:
                    line = line.decode().strip()
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP * LS :\r\n')
                    elif line.startswith('NICK '):
                        nick = line.split()[1]
                    elif line.startswith('USER '):
                        peer.sendall((f':mock 001 {nick} :Welcome\r\n'
                                      f':mock 005 {nick} CHANTYPES=# PREFIX=(ov)@+ :supported\r\n'
                                      f':mock 376 {nick} :End of MOTD\r\n').encode())
                    elif line.startswith('JOIN #test'):
                        peer.sendall((f':{nick}!u@host JOIN :#test\r\n'
                                      f':mock 353 {nick} = #test :{nick} sender\r\n'
                                      f':mock 366 {nick} #test :End of NAMES\r\n').encode())
                        # One record bigger than the client's socket-read budget.
                        payload = ''.join(f':sender!u@host PRIVMSG #test :message-{i:03d} '
                                          + 'x' * 100 + '\r\n' for i in range(100))
                        peer.sendall(payload.encode())
                        # No more writes: buffered messages must arrive without
                        # relying on a later packet to wake the client.
                    elif line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    workers = [threading.Thread(target=serve) for _ in range(2)]
    for worker in workers:
        worker.start()
    env = {**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config'),
           'SSL_CERT_FILE': certificate}
    clients = [subprocess.Popen([sys.argv[1], str(listener.getsockname()[1]), nick], env=env)
               for nick in ('receiver1', 'receiver2')]
    results = [client.wait(timeout=12) for client in clients]
    for worker in workers:
        worker.join(timeout=12)
    assert results == [0, 0], (results, errors)
    assert not errors, errors
