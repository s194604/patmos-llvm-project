; RUN: llc < %s -mpatmos-subfunction-align=32 -mpatmos-max-subfunction-size=64| FileCheck %s
; RUN: llc < %s -mpatmos-subfunction-align=32 -mpatmos-max-subfunction-size=64 -filetype=obj -o %t;\
; RUN: patmos-ld %t -nostdlib -static -o %t --section-start .text=1;\
; RUN: llvm-objdump %t -d| FileCheck %s --check-prefix ALIGN
; RUN: LLC_ARGS="-mpatmos-subfunction-align=32 -mpatmos-max-subfunction-size=64";\
; RUN: PASIM_ARGS="--mcsize=64"; %test_no_runtime_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////

;//////////////////////////////////////////////////////////////////////////////////////////////////

; CHECK: .p2align 5
; CHECK: .fstart	main, .Ltmp{{[0-9]}}-main, 32
; CHECK-NEXT: main:

; ALIGN: {{[0-9]*[0|2|4|6|8|a|c|e]4}} <main>:
define i32 @main()  {
entry:
  ; First 48 bytes
  %0 = call i32 asm "
      .inline_1:
		li		$0	=	5001		# Long arithmetic, 8 bytes each
		add		$0	=	$0, 5002
		add		$0	=	$0, 5003
		add		$0	=	$0, 5004
		add		$0	=	$0, 5005
		add		$0	=	$0, 5006
	", "=r"
	()
	
; CHECK: .p2align 5
; CHECK: .fstart	[[INLINE1:.LBB0_[0-9]]], .Ltmp{{[0-9]}}-[[INLINE1]], 32
; CHECK-LABEL: .inline_1:

; ALIGN: {{[0-9]*[0|2|4|6|8|a|c|e]4}} <.inline_1>:
  ; Then 16 bytes
  %1 = call i32 asm "
      .inline_2:
		add		$0	=	$1, 4
		add		$0	=	$0, 5
		add		$0	=	$0, 6
		add		$0	=	$0, 7	
	", "=r, r"
	(i32 %0)

; CHECK: .p2align 5
; CHECK: .fstart	[[INLINE2:.LBB0_[0-9]]], .Ltmp{{[0-9]}}-[[INLINE2]], 32
; CHECK-LABEL: .inline_2:

; ALIGN: {{[0-9]*[0|2|4|6|8|a|c|e]4}} <.inline_2>:
  ; Then another 36 bytes
  %2 = call i32 asm "
      .inline_3:
		add		$0	=	$1, 7
		add		$0	=	$0, 8
		add		$0	=	$0, 9	
		add		$0	=	$0, 10	
		add		$0	=	$0, 11	
		add		$0	=	$0, 12	
		add		$0	=	$0, 13	
		add		$0	=	$0, 14	
		add		$0	=	$0, 15	
	", "=r, r"
	(i32 %1)

; CHECK: .p2align 5
; CHECK: .fstart	[[INLINE3:.LBB0_[0-9]]], .Ltmp{{[0-9]}}-[[INLINE3]], 32	
; CHECK-LABEL: .inline_3:

; ALIGN: {{[0-9]*[0|2|4|6|8|a|c|e]4}} <.inline_3>:
  %3 = sub i32 %2, 30142
  ret i32 %3 ; =0
}
