; RUN: EXEC_ARGS="0=33 1=22 2=33 3=22"; \
; RUN: %test_execution
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests the ability to pass an i1 as an argument.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @helper(i1 %x) {
entry:
  %t = select i1 %x ,i32 22, i32 33
  ret i32 %t
}

define i32 @main(i32 %x) {
entry:
  %t = trunc i32 %x to i1
  %call = call i32 @helper(i1 %t)
  ret i32 %call
}

