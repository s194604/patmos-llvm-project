// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: ld.lld %t.patmos.o -o %t.patmos
// RUN: llvm-objdump -d %t.patmos | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Relocation Type R_PATMOS_CFLI_PCREL
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
    nop
    brnd foo

// CHECK:      04 c0 00 05     br   5
// CHECK:      04 c0 00 03     br   3
// CHECK:      04 bf ff fd     brnd -3