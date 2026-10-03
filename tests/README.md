# Library review tests

This standalone project tests the public C API (`uipc.h`, `upack.h`) and C++ wrapper (`uipc.hpp`) directly. It compiles `src/uipc.c` and `src/upack.c` without building or running sandbox programs. No library source, public header, or root build configuration changes are required. Existing working-tree changes are tested as they stand.

## Before submitting a feature or fix

Requirements: UNIX sockets, CMake 3.21+ for JUnit reports (3.20 for manual builds), C and C++20 compilers, pthreads, and Python 3. GCC or Clang with ASan/UBSan/LSan support is needed for memory checks. Linux is recommended; `/proc/self/fd` descriptor counts are automatically omitted on other systems. The bundled C++ utility headers require C++20.

From the repository root:

```sh
python3 tests/run.py
```

This runs **all cases, including stress tests**, in Debug, Release, and a Debug sanitizer build. Every profile gets its own build directory. The runner continues through failing profiles and returns nonzero if configuration, compilation, or any test fails. Checks remain active under `NDEBUG`; Release exercises library behavior without assertions. Do not interpret a passing Debug run as a passing review gate.

For targeted development:

```sh
python3 tests/run.py --profile debug --label server
python3 tests/run.py --profile debug --label client
python3 tests/run.py --profile sanitizer --label packet
python3 tests/run.py --profile sanitizer --label cpp
python3 tests/run.py --profile release --label stress
python3 tests/run.py --profile debug --label regression
```

Run the full command again before review. `--jobs 2` controls build and test parallelism; `--build-root /tmp/uipc-review` selects another artifact directory. Allow local UNIX socket operations in restricted environments; permissions failures are infrastructure failures, not test passes. No network services or downloads are used.

## Coverage and contracts

| Suite | Checks |
| --- | --- |
| C server | Bind/listen/address, duplicate binds, bad paths, cleanup, descriptor stability, raw-peer wire exchange, eight concurrent clients, 2,000 accepts |
| C client | Binary and empty messages, IDs and endian bytes, raw-server wire compatibility, bytewise fragmented headers/bodies, every truncated prefix, EOF, invalid lengths/version, missing-server retry, 2 MiB sends under backpressure, 100 MiB receive stress |
| Packets | Golden integer varints and compound bytes, signed limits, string size boundaries, empty containers, nested trees, array growth, missing keys, key ownership, replacing keys, depth 64, reproducible seeded trees, malformed input, 5,000 tree lifecycles and a 4 MiB string |
| C++ | Payload ownership, request/response metadata, empty payloads, version propagation, copy ownership, destructor/descriptor behavior, explicit close, real server/client wrappers, fragmented frames, raw reads, 2 MiB message transfer, missing servers, EOF handling, C packet interoperability, 5,000 exchanges, multiple translation unit linking |

Raw peers use independently specified wire bytes so matching client/server bugs cannot validate each other. Packet goldens similarly check compatibility independently of round trips. There is no separate C++ packet wrapper: packet coverage in C++ verifies the C packet API combined with `uipc::request`.

Some cases specify **desired hardening contracts** that the current API has not documented: malformed packet input should return `NULL` without aborting or reading past the buffer; C++ EOF should report an exception instead of dereferencing `NULL`; resource owners should prohibit shallow copies (messages may deep-copy). Server destruction should close and release owned resources. Discuss contract changes explicitly when implementing fixes; do not silently suppress these tests.

Each named case runs in its own process with a 60-second CTest timeout. A crash or sanitizer failure does not prevent other cases from reporting. Unique temporary socket directories allow parallel runs. Normal exits clean fixtures; a forced kill may leave a `uipc-tests-*` directory in `/tmp`. Timings use a monotonic clock. Stress tests assert data integrity and descriptor stability, while sanitizer runs check allocation lifetimes, out-of-bounds access, double frees, leaks and undefined behavior. They print workload sizes and timings rather than enforcing machine-dependent throughput thresholds. Compare timings on the same host/configuration; these are bounded stability workloads, not exhaustive fuzzing or a production capacity claim.

## Reports and manual runs

The runner writes timestamped artifacts under:

```text
build/library-tests/<profile>/reports/<UTC timestamp>/
    configure.log
    build.log
    test.log
    cases.log
    junit.xml
    summary.json
build/library-tests/latest.json
```

`latest.json` points to each profile's latest run. `summary.json` lists test names, status and seconds; JUnit XML can be uploaded to CI review reports. `test.log` contains failed checks, sanitizer stack traces, and linker errors. `cases.log` preserves complete per-case output, including passing stress timings. CTest retains complete per-case output in `<profile>/Testing/Temporary/LastTest.log`, including stress timings from passing cases. Timestamped reports survive reruns; CTest's temporary log is overwritten. Save or attach the reports to the review. No coverage percentage threshold is claimed.

Without Python:

```sh
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure --output-junit report.xml
ctest --test-dir build/tests -R '^client.fragmented$' -V
ctest --test-dir build/tests -L stress -V
```

Configure another directory with `-DCMAKE_BUILD_TYPE=Release`, or with `-DUIPC_SANITIZER=address` for ASan+UBSan and leak detection. The sanitizer instruments the library too, not just the harness. All cases are ordinary required tests: there are no expected-failure markers or leak suppressions. The multiple-translation-unit probe intentionally builds only when CTest runs it, so a header linker defect cannot prevent other tests from compiling.

The current implementation has failing malformed-packet, C++ ownership/lifecycle/version/EOF/linkage cases, plus sanitizer findings for empty containers/payloads, replaced packet children, and C++ path/server allocations. A nonzero full-suite result is expected until those library defects are fixed; these tests are intended to become the review gate after repairs.
