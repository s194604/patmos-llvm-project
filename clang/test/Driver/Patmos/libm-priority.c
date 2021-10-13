// RUN: %clang --target=patmos %s -o %t
// RUN: llvm-objdump -d -t %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that libm functions/symbols have priority over the same symbols defined in libc
//
///////////////////////////////////////////////////////////////////////////////////////////////////

int patmos_mock_libc_libm_shared_function();

// Get the address of 'PATMOS_MOCK_LIBM_INT' in symbol table
// CHECK: [[#%.1x,LIBM_INT_ADDR:]] {{[lg]}} O .data 00000004 PATMOS_MOCK_LIBM_INT

// CHECK-LABEL: <main>:
int main() { 
	return patmos_mock_libc_libm_shared_function(4);
}
// CHECK-LABEL: <patmos_mock_libc_libm_shared_function>:

// Ensure that libm's version of 'patmos_mock_libc_libm_shared_function' is used
// by checking that PATMOS_MOCK_LIBM_INT's address us used in its decimal form.
// CHECK: li [[RADDR:\$r[0-9]+]] = [[#%d,LIBM_INT_ADDR]]
// CHECK-NEXT: lwc [[RVAL:\$r[0-9]+]] = {{\[}}[[RADDR]]{{\]}}
// CHECK-NEXT: nop
// CHECK-NEXT: add $r1 = {{\$r[0-9]+}}, [[RVAL]]
