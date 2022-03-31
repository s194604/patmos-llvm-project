// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o
// RUN: not ld.lld %t.patmos.o --defsym foo=64 --defsym bar=-65 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_MEMB_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    lbs  $r1 = [$r2 + foo]
    lbl  $r1 = [$r2 + foo]
    lbc  $r1 = [$r2 + bar]
    lbm  $r1 = [$r2 + bar]

// CHECK: error
// CHECK: relocation Unknown (7) out of range: 64 is not in [-64, 63]
// CHECK: error
// CHECK: relocation Unknown (7) out of range: -65 is not in [-64, 63]