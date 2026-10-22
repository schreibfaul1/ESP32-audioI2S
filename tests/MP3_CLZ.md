# MP3 leading-zero regression

```sh
python3 tests/run_mp3_clz_test.py
```

The test compiles the actual `mp3_decoder/structs.h` with ASAN and UBSAN. A host
standard-integer header shim is the only adaptation. Zero must return 32; the
decoder's guard-bit callers then obtain 31 after subtracting one. Nonzero values
are checked against an independent bit-by-bit reference, including all one-bit
boundaries, signed bit patterns and 100000 deterministic pseudorandom values.

The old helper can be checked with
`--source-ref <pre-fix-commit>`; UBSAN must reject its `__builtin_clz(0)` call.
The test does not change DSP arithmetic or establish audio fidelity.
