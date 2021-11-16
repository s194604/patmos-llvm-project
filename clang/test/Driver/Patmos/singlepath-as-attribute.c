// RUN: %clang --target=patmos %s -S -emit-llvm -o - | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Test the "singlepath" attribute produces a function with a "sp-root" in its
// attribute list and "noinline".
//
///////////////////////////////////////////////////////////////////////////////////////////////////


// CHECK: @some_fn() #[[FN_ATTR:[0-9]]]
__attribute__((singlepath))
int some_fn()  {}

// CHECK: attributes #[[FN_ATTR]] = {
// CHECK-SAME: noinline 
// CHECK-SAME: "sp-root"