// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: ld.lld %t.patmos.o --defsym foo=16 --defsym bar=32 -o %t.patmos
// RUN: llvm-objdump -d %t.patmos | FileCheck %s

// Tests the minimal and the maximum value
// RUN: ld.lld %t.patmos.o --defsym foo=127 --defsym bar=-128 -o %t2.o
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Relocation Type R_PATMOS_MEMH_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    lhs  $r1 = [$r2 + foo]
    lhl  $r1 = [$r2 + foo]
    lhc  $r1 = [$r2 + bar]
    lhm  $r1 = [$r2 + bar]



// CHECK:      02 82 22 08     lhs  $r1 = [$r2 + 8]
// CHECK:      02 82 22 88     lhl  $r1 = [$r2 + 8]
// CHECK:      02 82 23 10     lhc  $r1 = [$r2 + 16]
// CHECK:      02 82 23 90     lhm  $r1 = [$r2 + 16]