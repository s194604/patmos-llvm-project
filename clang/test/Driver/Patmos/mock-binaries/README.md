# Mock Binaries

This folder contains mock binaries for testing that `clang` calls the other binaries in the toolchain correctly.

Each `.c` file is a mock binary that is compiled as part of test setup.
When the binary is called, it checks it arguments to see whether it received the correct inputs.
If not, it will print an error to stderr and return non-zero.

Each file may be compiled multiple times, once for each use case.
Each version then check for different arguments.

Tests must make `clang` call the mock binaries (which are found in the corresponding directory in the build directory and can be used as `%T/../mock-binaries/<mock-binary>`).