// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o
// RUN: not ld.lld %t.patmos.o --defsym foo=4096 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_ALUI_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    sl      $r1 = $r1, foo
    sr      $r1 = $r1, bar
    sra     $r1 = $r1, bar

.global bar
bar = 4096

// CHECK: error
// CHECK: relocation Unknown (3) out of range: 4096 is not in [0, 4095]