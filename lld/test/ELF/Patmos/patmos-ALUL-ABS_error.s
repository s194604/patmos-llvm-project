// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: not ld.lld %t.patmos.o --defsym foo=-1 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_ALUL_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    add     $r1 = $r1, foo
    sub     $r1 = $r1, bar
    xor     $r1 = $r1, foo
    or      $r1 = $r1, foo
    and     $r1 = $r1, foo
    nor     $r1 = $r1, foo
    shadd   $r1 = $r1, foo
    shadd2  $r1 = $r1, foo

.global bar
bar = 4294967296


// CHECK: error
// CHECK: relocation Unknown (5) out of range: 18446744073709551615 is not in [0, 4294967295]