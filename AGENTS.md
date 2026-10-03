# Repository Guidelines

## Project Structure & Module Organization
UIPC provides local inter-process communication through UNIX sockets. `src/uipc.c` implements sockets and messages; `src/upack.c` implements packet serialization. Public C headers and the C++ wrapper live in `include/uipc/`. Supporting headers are in `include/cpl-basics/` and `include/stb_ds.h`. `sandbox/src/` contains client, server, and packet examples. CMake places generated artifacts in `build/`; there is no separate asset directory.

## Build, Test, and Development Commands
Use CMake 3.20 or newer on a UNIX-compatible system. Sandbox programs require a C++23 compiler and standard library with `<print>` support.

- `cmake -S . -B build`: configure targets and generate `build/compile_commands.json`.
- `cmake --build build`: build the static C library and sandbox executables.
- `cmake --build build --target uipc`: build only the C library.
- `./build/sandbox/packet_test`: run the packet serialization example.
- `./build/sandbox/server`, then `./build/sandbox/client` in another terminal: exercise request/response communication. Enter a word in the client; enter `exit` to stop it.

Prefer CMake: the root Makefile references absent `server.c` and `client.c` files and has no library build rule.

## Coding Style & Naming Conventions
Match nearby code: four-space indentation, snake_case C functions and variables, and uppercase constants. Public C opaque types use names such as `PacketObject`; C++ wrappers use lowercase names in namespace `uipc` and `M_` member prefixes. Preserve the surrounding brace style, which differs between C functions and C++ methods. Keep public declarations in `include/uipc/` and implementations in `src/`. No formatter or linter configuration is checked in.

## Testing Guidelines
There is no dedicated test framework, CTest registration, or coverage threshold. `packet_test` is a manual example that writes `output.bin` and `output2.bin`; inspect its results rather than treating successful execution as assertion-based validation. For changes, rebuild affected targets and exercise relevant packet or client/server behavior. Add regression checks for serialization boundaries and socket failures when extending test coverage; use descriptive names ending in `_test` for new test targets.

## Commit & Pull Request Guidelines
The short Git history uses informal subjects such as `readme file`; no enforced convention is evident. Write concise, imperative subjects describing the change. Pull requests should explain the problem, affected APIs, and validation commands/results. Link relevant issues and describe protocol or ownership changes explicitly. Exclude generated build files and binary example output from commits.
