// RUN: export PATMOS_GOLD=%T/../mock-binaries/mock-patmos-illegal; \
// RUN: %clang --target=patmos %s -c -o %t
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that when producing an object file, the gold linker is not called.
//
// We test this by setting 'PATMOS_GOLD' to a program that always fails.
// If the linker would be called, the program would fail the build.
// 
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() { }