// RUN: export PATMOS_GOLD="invalid/patmos/gold/ld" ; \
// RUN: %clang --target=patmos %s 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the environment variable 'PATMOS_GOLD'
//
// We check it by setting the variable to an invalid path and checking that an error is returned.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() { }

// CHECK: error: gold linker specified through PATMOS_GOLD environment variable not found: 'invalid/patmos/gold/ld'
