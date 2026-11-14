"""Exercise authoritative WHO, away-notify and WHOIS state with a local peer."""
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
    listener.settimeout(12)
    observed, errors = [], []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(15)
                data = b''
                whois = {}
                while chunk := peer.recv(8192):
                    data += chunk
                    while b'\n' in data:
                        raw, data = data.split(b'\n', 1)
                        line = raw.rstrip(b'\r').decode()
                        observed.append(line)
                        if line.startswith('CAP LS'):
                            peer.sendall(b':mock CAP tester LS :away-notify\r\n')
                        elif line.startswith('CAP REQ'):
                            assert 'away-notify' in line
                            peer.sendall(b':mock CAP tester ACK :away-notify\r\n')
                        elif line.startswith('USER '):
                            peer.sendall(b':mock 001 tester :Welcome\r\n'
                                         b':mock 005 tester CHANTYPES=# PREFIX=(ov)@+ :supported\r\n'
                                         b':mock 376 tester :End of MOTD\r\n')
                        elif line == 'JOIN #presence':
                            peer.sendall(b':tester!u@host JOIN :#presence\r\n'
                                         b':mock 353 tester = #presence :@tester +bob alice\r\n'
                                         b':mock 366 tester #presence :End of NAMES\r\n'
                                         b':mock 352 tester #presence u h mock bob G+ :0 Bob\r\n'
                                         b':mock 352 tester #presence u h mock alice H :0 Alice\r\n'
                                         b':mock 315 tester #presence :End of WHO\r\n'
                                         b':mock 301 tester alice :Away reply to a message\r\n')
                        elif line == 'PRIVMSG control :begin':
                            peer.sendall(b':bob!u@host AWAY\r\n'
                                         b':outsider!u@host AWAY :Out for lunch\r\n')
                        elif line == 'PRIVMSG control :next':
                            peer.sendall(b':bob!u@host AWAY :Gone fishing\r\n'
                                         b':outsider!u@host AWAY\r\n')
                        elif line in ('WHOIS bob bob', 'WHOIS outsider outsider'):
                            nick = line.split()[1]
                            whois[nick] = whois.get(nick, 0) + 1
                            peer.sendall(f':mock 311 tester {nick} u h * :A person\r\n'.encode())
                            # Cross a snapshot tick before the optional away reply.
                            time.sleep(0.7)
                            if whois[nick] == 1:
                                peer.sendall(f':mock 301 tester {nick} :Gone fishing\r\n'.encode())
                            peer.sendall(f':mock 318 tester {nick} :End of WHOIS\r\n'.encode())
                            if nick == 'bob' and whois[nick] == 1:
                                peer.sendall(b':mock NOTICE tester :whois-away-done\r\n')
                        elif line == 'PRIVMSG control :disconnect':
                            return
                        elif line == 'AWAY :Out for lunch':
                            peer.sendall(b':mock 306 tester :You have been marked as being away\r\n')
                        elif line == 'AWAY':
                            peer.sendall(b':mock 305 tester :You are no longer marked as being away\r\n')
                        elif line.startswith('PING '):
                            peer.sendall(('PONG ' + line[5:] + '\r\n').encode())
        except Exception as error:
            errors.append(error)

    thread = threading.Thread(target=serve)
    thread.start()
    env = {**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config')}
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1])], env=env, timeout=45)
    thread.join(timeout=3)
    assert not thread.is_alive()
    assert not errors, (errors, observed)
    assert result.returncode == 0, (result.returncode, observed)
    assert any(line.startswith('CAP REQ') and 'away-notify' in line for line in observed)
    assert observed.count('AWAY :Out for lunch') == 2
    assert observed.count('AWAY') == 2
