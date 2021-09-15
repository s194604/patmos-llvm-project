; RUN: EXEC_ARGS="0=1 1=2 2=5 3=10 10=101"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests unsigned multiply where only the low-order output is used.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %x) {
entry:
  ; Test using just the low-order output
  %mul = mul nuw i32 %x, %x 
  %add = add nuw i32 %mul, 1
  
  ret i32 %add
}
