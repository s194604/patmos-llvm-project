; RUN: %test_no_runtime_execution

; Test that can also provide a clobber register with a preceding '$'

@_2 = global i32 2

define i32 @main() {
entry:
  %0 = load volatile i32, i32* @_2
  %1 = call i32 @dec_with_clob(i32 %0)
  ret i32 %1
}

; We use inline assembly in a different function
; such that we can try and mess with ABI-defined registers
define i32 @dec_with_clob(i32 %x) {
	; We try to clober r3, where x resides according to the ABI
	call void asm "
		li $$r3 = 15
		", "~{$r3}"()
	; We now use x. If the clober was handled, should result in 0, otherwise 13
	%1 = sub i32 %x, 2
	ret i32 %1
}
