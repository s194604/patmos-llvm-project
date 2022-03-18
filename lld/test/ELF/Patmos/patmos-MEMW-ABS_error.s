// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o
// RUN: not ld.lld %t.patmos.o --defsym foo=256 --defsym bar=-257 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_MEMW_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    lws  $r1 = [$r2 + foo]
    lwl  $r1 = [$r2 + foo]
    lwc  $r1 = [$r2 + bar]
    lwm  $r1 = [$r2 + bar]

// CHECK: error
// CHECK: relocation Unknown (9) out of range: 64 is not in [-64, 63]
// CHECK: error
// CHECK: relocation Unknown (9) out of range: -65 is not in [-64, 63]