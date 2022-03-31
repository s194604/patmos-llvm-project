// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o
// RUN: not ld.lld %t.patmos.o --defsym foo=128 --defsym bar=-129 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_MEMH_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    lhs  $r1 = [$r2 + foo]
    lhl  $r1 = [$r2 + foo]
    lhc  $r1 = [$r2 + bar]
    lhm  $r1 = [$r2 + bar]

// CHECK: error
// CHECK: relocation Unknown (8) out of range: 64 is not in [-64, 63]
// CHECK: error
// CHECK: relocation Unknown (8) out of range: -65 is not in [-64, 63]