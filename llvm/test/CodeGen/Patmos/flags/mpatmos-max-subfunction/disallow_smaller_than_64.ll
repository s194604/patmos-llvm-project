; RUN: llc < %s -mpatmos-max-subfunction-size=60 %XFAIL-filecheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that if the subfunction size is set to less than 64, an error is thrown
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main() {
entry:
  ; We make a block of 80 bytes, which can in no way fit in a 64 byte subfunction
  %0 = call i32 asm "
		li		$0	= 4096	# Long arithmetic, 8 bytes each
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
		li		$0	= 4096	
	", "=r"
	()
  
  ret i32 %0
}

; CHECK: Subfunction size less than 64 bytes
