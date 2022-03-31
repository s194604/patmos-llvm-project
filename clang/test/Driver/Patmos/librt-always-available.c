// RUN: %clang --target=patmos %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s
// RUN: %clang --target=patmos %s -o %t -O2
// RUN: llvm-objdump -d %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that the compiler-rt builtin functions (librt) are always available,
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
		"li $r1 = patmos_mock_librt_function;"
		"callnd $r1;"
		:
		:
		: "r1"
	);
}
// CHECK-DAG: <patmos_mock_librt_function>: