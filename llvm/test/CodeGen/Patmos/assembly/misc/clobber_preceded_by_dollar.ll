; RUN: llc < %s %XFAIL-filecheck %s
; END.
; Test that cannot provide clobbers preceded by '$'
;
; We test this because we previously allowed it.
; We not ensure a sensible error message is printed if anyone tries it.

define i32 @dec_with_clob(i32 %x) {
  call void asm "
    li $$r3 = 15
  ", "~{$r3}"()
  ; CHECK: Inline assembly clobbers cannot have '$' preceding clobbered registers
  %1 = sub i32 %x, 2
  ret i32 %1
}
