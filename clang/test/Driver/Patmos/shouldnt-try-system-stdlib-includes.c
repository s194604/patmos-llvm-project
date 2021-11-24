// RUN: %clang --target=patmos %s 2>&1 | FileCheck %s 
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that the system-installed standard library is never used as include path.
//
// We test this by including a stdlib header without adding any include paths.
// This should always fail, however, it should not fail with a message including
// the system header.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
// CHECK-NOT: /usr/include/stdlib.h
// CHECK: fatal error
int main() { }