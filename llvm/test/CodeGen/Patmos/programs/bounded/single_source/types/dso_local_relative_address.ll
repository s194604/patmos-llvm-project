; RUN: EXEC_ARGS="1=3 2=4 3=5 100=102"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests loading from a 'dso_local' global with a relative offset.
;
; We trigger this case by trying to load the second element of an i32 array,
; which must therefore get calculated as 'array + 4' to get right element.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@array = dso_local global [2 x i32] [i32 1, i32 2]

define i32 @main(i32 %x) {
entry:
  %second = getelementptr [2 x i32], [2 x i32]* @array, i32 0, i32 1
  %0 = load volatile i32, i32* %second
  %1 = add nsw i32 %x, %0
  ret i32 %1
}
