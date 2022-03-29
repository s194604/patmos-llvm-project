// RUN: %clang --target=patmos %s -o %t
// RUN: llvm-objdump -d -t %t | FileCheck %s
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that programs may define symbols that are already define in the standard library.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Define a function that uses the same name as a libc function.
int patmos_mock_libc_libm_shared_function() {
	return 8469;
}

// Get the address of 'patmos_mock_libc_libm_shared_function' in symbol table
// CHECK: [[#%.1x,FN_ADDR:]] {{[lg]}} F .text [[#%.8x,UNUSED:]] patmos_mock_libc_libm_shared_function

// CHECK-label: <main>:
int main() { 
	// CHECK: call{{(nd)?}} [[#%d,div(FN_ADDR,4)]]
	patmos_mock_libc_libm_shared_function();
}

// Ensure that the correct version of 'patmos_mock_libc_libm_shared_function' is used:
// CHECK: [[#%x,FN_ADDR]] <patmos_mock_libc_libm_shared_function
// CHECK-NEXT: li $r1 = 8469
// CHECK-NEXT: ret

