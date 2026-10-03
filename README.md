# UIPC - Unix Inter-Process Communication

A C library for communication between local processes using UNIX sockets, with C++ wrappers and structured packet serialization.

## Build

Requires CMake 3.20+, a UNIX-compatible system, and a C++17 compiler for the examples.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The `uipc` target builds the static C library. `uipc::c` and `uipc::cpp` are CMake aliases for consumers. Use CMake; the legacy Makefile does not describe the current source layout.

## Examples

Start the server, then run the client in another terminal:

```sh
./build/sandbox/server /tmp/uipc.socket
./build/sandbox/client /tmp/uipc.socket
```

Both default to `/tmp/uipc.socket` when no path is supplied. The client accepts complete lines, sends a compound packet containing `message`, and prints the echoed text. Enter `exit` or close stdin to stop the client. Connection and response failures produce a nonzero exit status.

For a single exchange, run `server /tmp/uipc.socket --once`; it exits and removes its socket after handling one connection. The server echoes request payloads unchanged. Receive operations reject truncated frames and lengths outside 6 bytes to 16 MiB. Stop the continuous server with Ctrl+C; remove its leftover socket before restarting.

`packet_test` checks nested compound/array string round trips, including empty strings, and verifies reserialized bytes. It is registered with CTest and creates no output files. Packet deserialization currently assumes trusted, valid packet contents; malformed packet validation remains future work.
