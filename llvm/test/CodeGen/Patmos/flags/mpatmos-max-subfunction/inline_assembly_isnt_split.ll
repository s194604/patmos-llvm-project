; RUN: LLC_ARGS="-mpatmos-max-subfunction-size=64"; \
; RUN: PASIM_ARGS="--mcsize=64"; \ 
; RUN: %test_no_runtime_execution
; RUN: llc < %s -mpatmos-max-subfunction-size=64 | FileCheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that even if splitting an inline assembly block would result in fewer subfunctions
; being needed, this is not done.
;
; Here we make 3 inline assembly blocks. The first takes up 24 bytes, which leaves 4 bytes free
; after adding the inter-block branch instruction.
; The second block is 12 bytes, while the third block plus the rest of the program takes 20 bytes.
; This means optimal block splitting can result in 2 subfunctions, if the second inline assembly 
; block is plit in 2. 
; We test that no blocks are split by seeing that all their instructions are scheduled right after
; each other. 
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; CHECK-LABEL: main
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
	
; CHECK-LABEL: .inline_1:
; CHECK: 	li		$r[[REG:[0-9]+]]	=	5001
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5002
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5003
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5004
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5005
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5006

  ; Then 16 bytes
  %1 = call i32 asm "
      .inline_2:
		add		$0	=	$1, 4
		add		$0	=	$0, 5
		add		$0	=	$0, 6
		add		$0	=	$0, 7	
	", "=r, r"
	(i32 %0)

; CHECK-LABEL: .inline_2:
; CHECK: 	add		$r[[REG:[0-9]+]]	=	$r{{[0-9]+}},	4
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		5
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		6
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		7

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
	
; CHECK-LABEL: .inline_3:
; CHECK: 	add		$r[[REG:[0-9]+]]	=	$r{{[0-9]+}},	7
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		8
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		9
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		10
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		11
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		12
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		13
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		14
; CHECK: 	add		$r[[REG]]			=	$r[[REG]],		15

  %3 = sub i32 %2, 30142
  ret i32 %3 ; =0
}
