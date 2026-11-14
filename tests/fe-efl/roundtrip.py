"""Exercise the real engine against a local, deterministic IRC peer."""
import os
import base64
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
    sasl = len(sys.argv) > 2 and sys.argv[2] == '--sasl'
    authenticated = []
    observed = []
    errors = []

    def serve():
        try:
            with listener.accept()[0] as peer:
                peer.settimeout(70)
                stream = peer.makefile('rb')
                welcomed = False
                while line := stream.readline():
                    line = line.decode().strip()
                    observed.append(line)
                    if line.startswith('CAP LS'):
                        peer.sendall(b':mock CAP tester LS :sasl=PLAIN\r\n' if sasl else b':mock CAP tester LS :\r\n')
                    if line.startswith('CAP REQ ') and 'sasl' in line:
                        peer.sendall(b':mock CAP tester ACK :sasl\r\n')
                    if line == 'AUTHENTICATE PLAIN':
                        peer.sendall(b'AUTHENTICATE +\r\n')
                    elif line.startswith('AUTHENTICATE '):
                        assert base64.b64decode(line.split(' ', 1)[1]) == b'testaccount\x00testaccount\x00testpass'
                        authenticated.append(True)
                        peer.sendall(b':mock 903 tester :SASL authentication successful\r\n')
                    if line.startswith('USER ') and not welcomed:
                        assert line.split()[1] == 'testuser', line
                        assert line.endswith(':A Person <person@example.org>'), line
                        assert not sasl or authenticated, 'Registration before SASL authentication'
                        welcomed = True
                        peer.sendall(b':mock 001 tester :Welcome\r\n'
                                     b':mock 005 tester CHANTYPES=# PREFIX=(ov)@+ :supported\r\n'
                                     b':mock 376 tester :End of MOTD\r\n')
                    if line.startswith('JOIN #test'):
                        peer.sendall(b':tester!u@host JOIN :#test\r\n'
                                     b':mock 353 tester = #test :zulu @tester +zelda +amber @zoe alice\r\n'
                                     b':mock 366 tester #test :End of NAMES\r\n'
                                     b':mock 332 tester #test :Initial <topic> & text\r\n'
                                     b':alice!u@host PRIVMSG #test :hello from server\r\n'
                                     b':alice!u@host PRIVMSG #test :\x01ACTION waves\x01\r\n'
                                     b':alice!u@host PRIVMSG tester :\x01ACTION smiles\x01\r\n'
                                     b':carol!u@host JOIN :#test\r\n'
                                     b':carol!u@host PART #test\r\n'
                                     b':dave!u@host JOIN :#test\r\n'
                                     b':dave!u@host NICK :david\r\n'
                                     b':david!u@host QUIT :Gone fishing\r\n'
                                     b':eve!u@host JOIN :#test\r\n'
                                     b':eve!u@host QUIT\r\n')
                    if line == 'JOIN #kicked':
                        peer.sendall(b':renamed!u@host JOIN :#kicked\r\n'
                                     b':operator!u@host KICK #kicked renamed :Goodbye\r\n'
                                     b':renamed!u@host JOIN :#remaining\r\n'
                                     b':alice!u@host PRIVMSG #remaining :still here\r\n')
                    if line == 'PRIVMSG #test :hello from UI':
                        peer.sendall(b':alice!u@host PRIVMSG #test :UI message received\r\n'
                                     b':bob!u@host JOIN :#test\r\n'
                                     b':mock MODE #test +v bob\r\n')
                        # Give the selected-channel snapshot time to include the join.
                        time.sleep(0.6)
                        peer.sendall(b':bob!u@host PART #test :Leaving\r\n')
                    if line == 'PRIVMSG #test :blocked message':
                        peer.sendall(b':alice!u@host PRIVMSG #test :tester: hello\r\n'
                                     b':alice!u@host PRIVMSG #test :hey TESTER!\r\n'
                                     b':alice!u@host PRIVMSG #test :testerBot\r\n'
                                     b':alice!u@host PRIVMSG #test :pretester\r\n'
                                     b':alice!u@host PRIVMSG #test :tester_\r\n'
                                     b':tester!u@host PRIVMSG #test :tester own message\r\n'
                                     b':alice!u@host NOTICE #test :tester notice\r\n'
                                     b':alice!u@host PRIVMSG #test :\x01ACTION greets tester\x01\r\n'
                                     b':alice!u@host JOIN :#test\r\n'
                                     b':alice!u@host PART #test :Taking a break\r\n'
                                     b':alice!u@host QUIT :See you\r\n'
                                     b':alice!u@host JOIN :#test\r\n'
                                     b':mock 404 tester #test :You need voice (+m)\r\n'
                                     b':mock 404 tester #unjoined :Cannot send to channel\r\n')
                    if line.startswith('TOPIC #test :'):
                        peer.sendall((':tester!u@host ' + line + '\r\n').encode())
                    if line == 'MODE #test +b *!*@bad.example':
                        peer.sendall(b':tester!u@host MODE #test +b *!*@bad.example\r\n')
                    if line == 'WHOIS alice alice':
                        peer.sendall(b':mock 311 tester alice user host * :Alice <alice@example.org>\r\n'
                                     b':mock 318 tester alice :End of WHOIS\r\n')
                    if line == 'PRIVMSG alice :\x01VERSION\x01':
                        peer.sendall(b':alice!u@host NOTICE tester :\x01VERSION mock-client\x01\r\n'
                                     b':alice!u@host NOTICE tester :\x01TIME Thu Nov 13 12:34:56 2026\x01\r\n'
                                     b':alice!u@host NOTICE tester :\x01PING junk\x01\r\n')
                    if line.startswith('PRIVMSG alice :\x01PING '):
                        payload = line.split(' :', 1)[1]
                        time.sleep(0.02)
                        peer.sendall((':alice!u@host NOTICE tester :' + payload + '\r\n').encode())
                    if line in ('PRIVMSG Missing :hello', 'PRIVMSG Missing :again'):
                        peer.sendall(b':mock 401 tester missing :No such nick\r\n')
                        if line.endswith(':hello'):
                            peer.sendall(b':mock 401 tester unrelated :No such nick\r\n')
                    if line == 'NICK renamed':
                        peer.sendall(b':tester!u@host NICK :renamed\r\n')
                    if line.startswith('PART #test '):
                        peer.sendall(b':renamed!u@host PART #test :Leaving\r\n'
                                     b':alice!u@host PRIVMSG #test :late channel traffic\r\n'
                                     b':bob!u@host JOIN :#test\r\n'
                                     b':bob!u@host PART #test :late part\r\n'
                                     b':mock NOTICE renamed :server status message\r\n')
                    if line.startswith('QUIT '):
                        break
        except Exception as error:
            errors.append(error)

    worker = threading.Thread(target=serve)
    worker.start()
    result = subprocess.run([sys.argv[1], str(listener.getsockname()[1])] + (['--sasl'] if sasl else []),
                            env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config')}, timeout=80)
    worker.join(timeout=13)
    assert result.returncode == 0, (result.returncode, observed, errors)
    assert not sasl or authenticated, observed
    assert 'PRIVMSG #test :hello from UI' in observed, observed
    assert 'PRIVMSG #test :\x01ACTION waves back\x01' in observed, observed
    assert 'PRIVMSG alice :\x01ACTION smiles back\x01' in observed, observed
    assert 'TOPIC #test :A test topic' in observed, observed
    assert 'TOPIC #test :' in observed, observed
    assert 'KICK #test zulu :Test reason' in observed, observed
    assert 'MODE #test +b *!*@bad.example' in observed, observed
    assert 'MODE #test -b *!*@bad.example' in observed, observed
    for mode, nick in [('+o', 'alice'), ('-o', 'zoe'), ('+v', 'zulu'), ('-v', 'amber')]:
        assert f'MODE #test {mode} {nick}' in observed, observed
    assert 'WHOIS alice alice' in observed, observed
    assert 'PRIVMSG alice :\x01VERSION\x01' in observed, observed
    assert 'NICK renamed' in observed, observed
    assert 'PART #test :Leaving' in observed, observed
    assert 'QUIT :Server closed' in observed, observed
    assert not errors, errors
