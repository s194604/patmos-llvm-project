; RUN: EXEC_ARGS="1=1 2=0 3=1 11=1 100=0"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests a situation where an integer is truncated into an i1, compared with something,
; and the result used as a branch condition.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %x) {
entry:
  %t = trunc i32 %x to i1
  %tobool = icmp ne i1 %t, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %if.then, %entry
  ret i32 0
}
