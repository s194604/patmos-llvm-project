; RUN: EXEC_ARGS="0=10 1=1 2=12 4=14"; \
; RUN: %test_execution
; END. 

; Helper for adding to double
define double @add_int(double %rhs, i64 %lhs) {
  %rhs_64 = bitcast double %rhs to i64
  %added = add i64 %rhs_64, %lhs
  %added_d = bitcast i64 %added to double
  ret double %added_d
}
; Helper for casting to i32
define i32 @to_i32(double %x) {
  %x_64 = bitcast double %x to i64
  %x_32 = trunc i64 %x_64 to i32
  ret i32 %x_32
}

define i32 @main(i32 %x) {
entry:
  %rem = urem i32 %x, 2
  %is_even = icmp eq i32 %rem, 0
  %x_64 = zext i32 %x to i64
  %x_d = bitcast i64 %x_64 to double
  br i1 %is_even, label %if.then, label %if.else

if.then:
  %add = call double @add_int(double %x_d, i64 10)
  br label %cleanup

if.else:
  switch i32 %x, label %unreach [
    i32 1, label %cleanup
  ]

unreach:
  unreachable

cleanup:
  %retval.0 = phi double [ %add, %if.then ], [ %x_d, %if.else ]
  %result = tail call i32 @to_i32(double %retval.0)
  ret i32 %result
}