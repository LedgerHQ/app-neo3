# Fuzzing

Building the fuzzer requires Clang and CMake.

To quickly get started fuzzing NEO transaction parser using libFuzzer:

```shell
cd tests/fuzzing
./build.sh
./run.sh
```

## Code coverage

To generate a code coverage report of the fuzzer, it is possible to use `llvm-cov` (on Ubuntu: `sudo apt install llvm`):

```shell
cd tests/fuzzing
./coverage.sh
```

These commands generate a HTML report in `tests/fuzzing/html-coverage/index.html`.
