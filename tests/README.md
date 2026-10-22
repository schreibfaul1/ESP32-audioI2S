# Host regression tests

Run with Python 3 and a Clang toolchain supporting C++20, AddressSanitizer and
UndefinedBehaviorSanitizer:

```sh
python3 tests/run_host_tests.py
```

The lifecycle test compiles the actual FAAD implementation and pointer owner.
It requires no outstanding tracked allocation after every sequential or
interleaved two-instance Open/Init2/Close lifecycle, in floating-point and
fixed-point profiles, with PSRAM-present and internal-RAM fallback adapters.
Repeated cleanup follows the wrapper's contract: clear the handle after Close;
a second reset closes NULL, never a dangling handle.

The host overlay replaces Arduino/platform logging only. The FAAD `.cpp` is
unchanged. The `NeAACDecInit` declaration is adapted from `long` to `int32_t` to
match its existing definition on LP64 hosts. Clang's delayed template parsing
avoids an unrelated, unused pointer-template method. ASAN/UBSAN check memory
access; an allocation ledger checks leaks and rejects unknown/double frees.
LeakSanitizer is disabled for macOS compatibility. This is not a device, task
scheduling, audio-output or allocation-failure test.

To reproduce the floating-point FFT table leak on a pre-fix revision:

```sh
python3 tests/run_host_tests.py --source-ref <pre-fix-commit> --expect-leak
```

That control must find 100 allocations of each 512-, 4096- and 2048-byte table
(665600 bytes total), then discards the intentionally leaked tables. The
fixed-point control must remain leak-free because its tables are static.
