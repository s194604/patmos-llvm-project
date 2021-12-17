; RUN: %test_no_runtime_execution

; Test can clobber a register

@_1 = global i32 1

define i32 @main() {
entry:
  %0 = load volatile i32, i32* @_1
  %1 = call i32 @dec_with_clob(i32 %0)
  ret i32 %1
}

; We use inline assembly in a different function
; such that we can try and mess with ABI-defined registers
define i32 @dec_with_clob(i32 %x) {
  ; We try to clober r1 andr3, where the return value or x resides according to the ABI
  ; This ensures regardless of where the subtraction happens, the clobbers will
  ; affect the output if not managed
  call void asm sideeffect "
    li $$r1 = 20
    li $$r3 = 10
    ", "~{r1},~{r3}"()
  ; Only if x has been saved before this, or %1 didn't use r1 before the inline assembly,
  ; will 0 be returned.
  %1 = sub i32 %x, 1
  ret i32 %1
}
