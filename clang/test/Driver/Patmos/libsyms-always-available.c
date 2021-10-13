// RUN: %clang --target=patmos %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s
// RUN: %clang --target=patmos %s -o %t -O2
// RUN: llvm-objdump -d %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that symbols in the libsyms.lst are always available,
// even if not used directly in the code.
//
// We test this by calling a librt function from inline assembly
// which is only resolved at link time.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <main>:
void main() { 
	asm volatile (
		// CHECK: li $r1 = 
		"li $r1 = patmos_mock_libc_function;"
		// CHECK-NEXT: li $r1 = 
		"li $r1 = patmos_mock_libm_function;"
		// CHECK-NEXT: li $r1 = 
		"li $r1 = patmos_mock_crt0_function;"
		:
		:
		: "r1"
	);
}
// CHECK-DAG: <patmos_mock_libc_function>:
// CHECK-DAG: <patmos_mock_libm_function>:
// CHECK-DAG: <patmos_mock_crt0_function>: