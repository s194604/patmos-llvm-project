// RUN: %clang --target=patmos %s -c -o %t
// RUN: llvm-objdump -d %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Test that the '-c' flag can be used to produce an object file.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK: <main>:
int main() {
// CHECK: retnd
}

// Ensure no std or builtin libs are linked
// CHECK-NOT: <patmos_mock_crt0_function>:
// CHECK-NOT: <patmos_mock_crtbegin_function>:
// CHECK-NOT: <patmos_mock_crtend_function>:
// CHECK-NOT: <patmos_mock_librt_function>:
// CHECK-NOT: <patmos_mock_libc_function>:
// CHECK-NOT: <patmos_mock_libpatmos_function>:

