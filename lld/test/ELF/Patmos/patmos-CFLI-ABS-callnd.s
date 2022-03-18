// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: ld.lld %t.patmos.o --defsym foo=0 --defsym bar=8 -o %t.patmos
// RUN: llvm-objdump -d %t.patmos | FileCheck %s

// Tests the maximum value (because values are unsigned)
// RUN: ld.lld %t.patmos.o --defsym foo=16777215 --defsym bar=8 -o %t2

// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Relocation Type R_PATMOS_CFLI_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    call    foo
    call    bar

// CHECK:      04 40 00 00     call   0
// CHECK:      04 40 00 02     call   2