# Mock Standard Libraries

This folder manages the setup of a mock standard library installation.
Each .c file corresponds to a .o file expected to be present in standard installation.
The files are compiled and in `<BUILD_DIR>/patmos-unknown-unknown-elf/lib`, so that clang can correctly find them using relative paths.

The files contain unique functions/variable that aren't available in the real versions. 
This is used to check that the libraries are correctly linked into executable.