; RUN: LLC_ARGS="-mpatmos-max-subfunction-size=64"; \
; RUN: PASIM_ARGS="--mcsize=64"; \ 
; RUN: %test_no_runtime_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that setting the subfunction size to the same as the method cache size will work 
; successfully.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_1 = global i32 1
@_11 = global i32 11

; We make a function that is larger than 64 bytes, and cannot be
; optimized to a smaller size. This ensures the function will
; only execute correctly if it is split correctly.
define i32 @main()  {
entry:
  %_1 = load volatile i32, i32* @_1
  %0 = add i32 1, %_1
  %_2 = load volatile i32, i32* @_1
  %1 = add i32 %0, %_2
  %_3 = load volatile i32, i32* @_1
  %2 = add i32 %1, %_3
  %_4 = load volatile i32, i32* @_1
  %3 = add i32 %2, %_4
  %_5 = load volatile i32, i32* @_1
  %4 = add i32 %3, %_5
  %_6 = load volatile i32, i32* @_1
  %5 = add i32 %4, %_6
  %_7 = load volatile i32, i32* @_1
  %6 = add i32 %5, %_7
  %_8 = load volatile i32, i32* @_1
  %7 = add i32 %6, %_8
  %_9 = load volatile i32, i32* @_1
  %8 = add i32 %7, %_9
  %_10 = load volatile i32, i32* @_1
  %9 = add i32 %8, %_10
  %_11 = load volatile i32, i32* @_11
  %10 = sub i32 %9, %_11
  ret i32 %10 ; =0
}

