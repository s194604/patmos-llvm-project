; RUN: EXEC_ARGS="0=0 1=0 2=1 3=2 10=9"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests unsigned multiply where only the high-order output is used.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %x) {
entry:
  %x_ext = zext i32 %x to i64
  %mul = mul nsw i64 %x_ext, 4294967295
  %mul_split = bitcast i64 %mul to <2 x i32>
  %mul_high = extractelement <2 x i32> %mul_split, i32 0
    
  ret i32 %mul_high
}
