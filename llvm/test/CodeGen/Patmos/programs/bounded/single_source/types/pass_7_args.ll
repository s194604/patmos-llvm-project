; RUN: EXEC_ARGS="0=20 1=21 2=22 3=23 4=24 5=25"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Test passing more than 6 arguments to a function.
; This causes the 7th argument to be passed using the stack, as per our calling convention.
;
; We don't test that the 7th argument is on the stack, just that it is possible to pass >6 arguments.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @helper([6 x i32] %array, i32 %idx) {
  %array_p = alloca [6 x i32]
  store [6 x i32] %array, [6 x i32]* %array_p
  %idx_p = getelementptr [6 x i32], [6 x i32]* %array_p, i32 0, i32 %idx
  %v = load i32, i32* %idx_p
  ret i32 %v
}

define i32 @main(i32 %x) {
  %1 = call i32 @helper([6 x i32] [i32 20, i32 21, i32 22, i32 23, i32 24, i32 25], i32 %x)
  ret i32 %1
}

