// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %s -o %t.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs255.s -o %t255.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs256.s -o %t256.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs257.s -o %t257.o
// RUN: ld.lld %t.o %t255.o -o /dev/null
// RUN: not ld.lld %t.o %t257.o -o /dev/null 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_ABS_32
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.globl _start
_start:
.data
  .word foo + 0xfffffeff
  .word foo - 0x80000100

// CHECK: error
// CHECK: relocation Unknown (10) out of range: 4294967296 is not in [-2147483648, 4294967295]