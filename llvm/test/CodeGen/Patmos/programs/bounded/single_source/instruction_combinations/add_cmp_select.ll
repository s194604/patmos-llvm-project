; RUN: EXEC_ARGS="0=0 1=2 2=0 3=6 4=0 5=10"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests a situation where a select depends through its condition on a parametera and through
; one of the selected values, which should use the parameter twice.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %_d) {
entry:     
  ; uses the parameter twice     
  %double = add i32 %_d, %_d
  ; Uses the parameter once
  %is_odd = trunc i32 %_d to i1
  ; Uses the parameter through condition and twice through val
  %res = select i1 %is_odd, i32 %double, i32 0   
  ret i32 %res
}
