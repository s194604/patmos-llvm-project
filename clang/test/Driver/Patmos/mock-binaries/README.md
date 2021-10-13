# Mock Binaries

This folder contains mock binaries for testing that `clang` calls the other binaries in the toolchain correctly.

Each `.c` file is a mock binary that is compiled as part of test setup.

Tests must make `clang` call the mock binaries (which are found in the corresponding directory in the build directory and can be used as `%T/../mock-binaries/<mock-binary>`).