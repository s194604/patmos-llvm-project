; RUN: EXEC_ARGS="0=0 1=0 2=1 3=1 10=5 23=11"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests unsigned multiply where both low- and high-order output is used
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; i32 MAX/2
@mask = global i32 2147483648

define i32 @main(i32 %x) {
entry:
  %x_ext = zext i32 %x to i64
  %0 = load volatile i32, i32* @mask
  %mask_ext = zext i32 %0 to i64
  
  %mul = mul nsw i64 %x_ext, %mask_ext
  %mul_split = bitcast i64 %mul to <2 x i32>
  %mul_high = extractelement <2 x i32> %mul_split, i32 0
  %mul_low = extractelement <2 x i32> %mul_split, i32 1
  %mul_add = add nsw i32 %mul_high, %mul_low
  
  ret i32 %mul_add
}
