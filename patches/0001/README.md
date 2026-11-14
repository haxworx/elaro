# 0001 — Deliver buffered TLS data without waiting for another socket event

Patch: [`irssi-tls-buffered-read.patch`](irssi-tls-buffered-read.patch)

Targets the official **Irssi 1.4.5** source release. Elaro already contains this
fix; do not apply the patch to Elaro itself.

## Problem

An IRC message burst can stop arriving at the parser even though OpenSSL has
already decrypted the remaining messages. Irssi intentionally limits reads per
dispatch, but its TLS watch checks only the underlying socket. Once the socket
is empty, buffered plaintext does not trigger another dispatch, leaving messages
stuck until more network traffic arrives.

## Fix

Give the SSL channel its own GLib source that checks both socket readiness and
`SSL_pending()`. This preserves bounded reads and holds a channel reference for
the lifetime of the watch.

The patch includes an independent C/Python regression and Meson integration.
Two Irssi core processes connect to a local TLS peer, which sends each client
100 ordered messages in one TLS record and then sends nothing else. Each client
must receive the complete burst. Certificate verification remains enabled with
an explicitly trusted temporary certificate.

## Apply and verify

From a fresh Irssi 1.4.5 source directory, using the downloaded patch's path:

```sh
patch -p1 < /path/to/patches/0001/irssi-tls-buffered-read.patch
meson setup Build -Dwith-perl=no -Dwith-otr=no -Dwithout-textui=yes
meson compile -C Build
meson test -C Build 'TLS buffered receive' --print-errorlogs
meson test -C Build --print-errorlogs
```

The regression requires Python 3, the `openssl` command and loopback socket
access. Meson skips it if the optional programs are unavailable.

## Validation

The recorded reproduction on pinned upstream master
`43f1727ef9fe1cb51d5031ee2dd0e1965217e4cb` stalled at 71/100 messages in both
clients before the fix. Both clients received 100/100 afterward. The patched
master and 1.4.5 headless builds each passed all five tests.

This exported patch is the release-specific version, verified against the
unchanged official 1.4.5 archive. It can be followed by patch 0002. Nothing has
been submitted upstream.
