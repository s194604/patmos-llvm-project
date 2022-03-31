; RUN: EXEC_ARGS="1=11 2=11 3=13 4=13 5=11 7=15 8=17 9=19 10=11"; \
; RUN: %test_execution
; END. 
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests a "select" instruction inside a loop, whose condition and at least 1 operand come from
; phi instructions.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %x) {
entry:
  br label %for.body

for.body:                                      
  %0 = phi i1 [ 1, %for.body ], [ 0, %entry ]
  %1 = phi i32 [ %add.x, %for.body ], [ %x, %entry ]
  %2 = select i1 %0, i32 %1, i32 1 
  %above10 = icmp ugt i32 %2, 10
  %add.x = add i32 %2, %x
  br i1 %above10, label %end, label %for.body, !llvm.loop !0

end:                         
  ret i32 %2
}

!0 = !{!0, !1}
!1 = !{!"llvm.loop.bound", i32 1, i32 10}