// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

// RUN: ld.lld %t.patmos.o --defsym foo=2 -o %t.patmos
// RUN: llvm-objdump -d %t.patmos | FileCheck %s   

// Tests the maximum value (because values are unsigned)
// RUN: ld.lld %t.patmos.o --defsym foo=4095 -o %t2.o
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests the Relocation Types R_PATMOS_ALUI_ABS and R_PATMOS_ALUL_ABS
//
///////////////////////////////////////////////////////////////////////////////////////////////////

.global _start
_start:
    add     $r1 = $r1, foo
    sub     $r1 = $r1, bar
    xor     $r1 = $r1, foo
    sl      $r1 = $r1, foo
    sr      $r1 = $r1, foo
    sra     $r1 = $r1, foo
    or      $r1 = $r1, foo
    and     $r1 = $r1, foo
    nor     $r1 = $r1, foo
    shadd   $r1 = $r1, foo
    shadd2  $r1 = $r1, foo

.global bar
bar = 255

// CHECK:      87 c2 10 00 00 00 00 02 add $r1 = $r1, 2
// CHECK:      87 c2 10 01 00 00 00 ff sub $r1 = $r1, 255
// CHECK:      87 c2 10 02 00 00 00 02 xor $r1 = $r1, 2
// CHECK:      00 c2 10 02 sl $r1 = $r1, 2
// CHECK:      01 02 10 02 sr $r1 = $r1, 2
// CHECK:      01 42 10 02 sra $r1 = $r1, 2
// CHECK:      87 c2 10 06 00 00 00 02 or $r1 = $r1, 2
// CHECK:      87 c2 10 07 00 00 00 02 and $r1 = $r1, 2
// CHECK:      87 c2 10 0b 00 00 00 02 nor $r1 = $r1, 2
// CHECK:      87 c2 10 0c 00 00 00 02 shadd $r1 = $r1, 2
// CHECK:      87 c2 10 0d 00 00 00 02 shadd2 $r1 = $r1, 2