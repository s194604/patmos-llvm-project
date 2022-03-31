// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: ld.lld %t.patmos.o --defsym foo=16 --defsym bar=32 -o %t.patmos
// RUN: llvm-objdump -d %t.patmos | FileCheck %s

// Tests the minimal and the maximum value
// RUN: ld.lld %t.patmos.o --defsym foo=63 --defsym bar=-64 -o %t2.o
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Relocation Type R_PATMOS_MEMB_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    lbs  $r1 = [$r2 + foo]
    lbl  $r1 = [$r2 + foo]
    lbc  $r1 = [$r2 + bar]
    lbm  $r1 = [$r2 + bar]

// CHECK:      02 82 24 10     lbs  $r1 = [$r2 + 16]
// CHECK:      02 82 24 90     lbl  $r1 = [$r2 + 16]
// CHECK:      02 82 25 20     lbc  $r1 = [$r2 + 32]
// CHECK:      02 82 25 a0     lbm  $r1 = [$r2 + 32]