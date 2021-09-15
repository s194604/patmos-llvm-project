; RUN: llc < %s -mpatmos-max-subfunction-size=64 | FileCheck %s
; RUN: llc < %s -mpatmos-max-subfunction-size=64 -filetype=obj -o %t --basicblock-sections=labels;\
; RUN: llvm-objdump %t -d -t | FileCheck %s --check-prefix OBJ
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that assembly and object outputs give subfunction labels the `STT_FUNC` symbol type.
;
; Using the "CHECK" filecheck prefix, we ensure assembly output adds the '@function' attribute to subfunctions.
; Using the "OBJ" filecheck prefix, we ensure that the labels of the subfunctions are local ('l') and functions ('F') in the symbol table.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; CHECK: .type main,@function
; CHECK: main:
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
	
; CHECK: .type {{.LBB0_[0-9]}},@function
  ; Then 18 bytes
  %1 = call i32 asm "
      .inline_2:
		add		$0	=	$1, 4
		add		$0	=	$0, 5
		add		$0	=	$0, 6
		add		$0	=	$0, 7	
	", "=r, r"
	(i32 %0)

; CHECK: .type {{.LBB0_[0-9]}},@function
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

; CHECK: .type {{.LBB0_[0-9]}},@function
  %3 = sub i32 %2, 30142
  ret i32 %3 ; =0
}

; The following checks the type of the labels in the symbol table

; We first find the addresses of the labels in our inline assembly.
; OBJ: [[ADDRESS1:[[:xdigit:]]{8}]] l .text {{([[:xdigit:]]{8})}} .inline_1
; OBJ: [[ADDRESS2:[[:xdigit:]]{8}]] l .text {{([[:xdigit:]]{8})}} .inline_2
; OBJ: [[ADDRESS3:[[:xdigit:]]{8}]] l .text {{([[:xdigit:]]{8})}} .inline_3

; We then check that these addresses are assigned with a label that is 'l' and 'F'
; OBJ: [[ADDRESS1]] l F .text
; OBJ: [[ADDRESS2]] l F .text
; OBJ: [[ADDRESS3]] l F .text
; OBJ: g F .text {{([[:xdigit:]]{8})}} main
