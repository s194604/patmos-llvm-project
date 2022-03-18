// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o
// RUN: not ld.lld %t.patmos.o --defsym foo=16777216 --defsym bar=8 -o %t.patmos 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_CFLI_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    call    foo
    call    bar

.global bar
bar = -5

// CHECK: error
// CHECK: relocation Unknown (1) out of range: 4194304 is not in [0, 4194303]