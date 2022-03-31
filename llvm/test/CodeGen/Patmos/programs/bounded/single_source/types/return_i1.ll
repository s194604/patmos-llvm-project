; RUN: EXEC_ARGS="0=0 1=1 2=0 3=1 11=1 100=0"; \
; RUN: %test_execution
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests the ability to return an i1. Known to not work.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i1 @helper(i32 %x) {
entry:
  %t = trunc i32 %x to i1
  ret i1 %t
}

define i32 @main(i32 %x) {
entry:
  %t = call i1 @helper(i32 %x)
  %ext = zext i1 %t to i32
  ret i32 %ext
}

