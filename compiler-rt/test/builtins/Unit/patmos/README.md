# Patmos Builitins Tests

Tests of the Patmos builtins.

Each `.c` file is a test of a builtin function.
The tests can use the `llvm-lit` `%test-patmos-librt` command to execute and must have a `main` function that takes no arguments and returns 0 upon success.
`%test-patmos-librt` compiles the file and links it with the Patmos `librt` (and a start function) and executes it on `pasim`.
Upon failure, the tests should use the return code as a guide to where the error occurred.