// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: not ld.lld %t.patmos.o -o %t.patmos --defsym bar=16777216 2>&1 | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Error for Out of Bound Values of Relocation Type R_PATMOS_CFLI_PCREL
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    br    bar
    br    foo
    nop
    nop    

.global foo, bar
foo:  
    nop
bar:
    nop

// CHECK: error
// CHECK: relocation Unknown (12) out of range: 4160467 is not in [-2097152, 2097151]