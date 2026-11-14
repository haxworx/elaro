Memory checks can be run with a separate AddressSanitizer build:

```sh
meson setup build-asan -Db_sanitize=address -Db_lundef=false -Dbuildtype=debug
meson compile -C build-asan
ASAN_OPTIONS=detect_leaks=1 meson test -C build-asan --no-rebuild \
  'EFL conversations and user state' 'EFL engine round trip' 'EFL engine early shutdown'
```

The UI test repeatedly opens and closes a channel with 500 users, 600 messages
and 250 sent entries per cycle. It exercises scrollback trimming, input history
limits, drafts, selection changes and user-list cleanup. The engine test covers
leaving, late traffic after leaving, and being kicked. The engine round trip
requires permission to open a loopback socket. LeakSanitizer requires permission
to inspect the test process; restricted sandboxes may prevent it from running.

The locally installed EFL 1.29.99 library reports leaks in
`_evas_object_smart_clipped_init` and `evas_module_find_type`. These are library
allocations, including a small block per deleted widget. They remain unresolved
by changes to Elaro. To check for additional leaks while using that build,
repeat the command with:

```sh
LSAN_OPTIONS="suppressions=$PWD/tests/fe-efl/lsan-efl.supp" \
ASAN_OPTIONS=detect_leaks=1 meson test -C build-asan --no-rebuild \
  'EFL conversations and user state' 'EFL engine round trip' 'EFL engine early shutdown'
```

The suppressions are optional and are never enabled by the test definitions.
They name only the two EFL functions, leaving Elaro's own allocations checked.
Always inspect an unsuppressed report first when using another EFL version.

Channel browser and ban manager memory checks use the same build and optional
EFL suppressions:

```sh
ASAN_OPTIONS=detect_leaks=1 meson test -C build-asan --no-rebuild \
  'Channel list window' 'Channel bans window' \
  'IRC channel list' 'Channel ban management'
```

The window tests repeatedly close populated and still-loading windows, including
queued channel rows and late replies after closing. The IRC tests exercise list
completion, refreshes, errors and exact ban-mask removal.

`Two clients receive complete TLS bursts` starts two separate engine processes
against a local TLS IRC peer. Each receives 100 ordered channel messages in a
single TLS record, followed by no further server traffic. This checks that
OpenSSL's buffered plaintext wakes the engine after its bounded read batch,
without waiting for another socket packet. The test uses the `openssl` command
to generate a temporary certificate trusted only by the test processes through
`SSL_CERT_FILE`; normal certificate verification stays enabled.
