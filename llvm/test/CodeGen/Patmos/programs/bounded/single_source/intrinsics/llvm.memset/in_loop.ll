; RUN: EXEC_ARGS="0=0 1=0 2=1 26=25"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests converting llvmset that is in the header of a loop
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %iter_count.0)  {
entry:
  %ptr = alloca i8, i32 28, align 4
  %iter_count = trunc i32 %iter_count.0 to i8
  br label %loop
  
loop:
  %i = phi i8 [0, %entry], [%i.inc, %loop]
  call void @llvm.memset.p0i8.p0i8.i32(i8* %ptr, i8 %i, i32 28, i1 false)
  %i.inc = add i8 %i, 1
  %repeat = icmp ult i8 %i.inc, %iter_count
  br i1 %repeat, label %loop, label %end, !llvm.loop !0

end:
  %ptr.end = getelementptr i8, i8* %ptr, i8 %iter_count
  %val = load volatile i8, i8* %ptr.end
  %val.i32 = zext i8 %val to i32
  ret i32 %val.i32
}
declare void @llvm.memset.p0i8.p0i8.i32(i8*, i8, i32, i1)

!0 = !{!0, !1}
!1 = !{!"llvm.loop.bound", i32 0, i32 50}