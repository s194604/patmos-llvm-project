; RUN: EXEC_ARGS="0=0 1=1 2=3 14=27"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests converting llvmset that is in the header of a loop
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %val)  {
entry:
  %ptr.src = alloca i8, i32 15, align 4
  %ptr.dst = alloca i8, i32 15, align 4
  %val.i8 = trunc i32 %val to i8
  br label %loop
  
loop:
  %i = phi i8 [0, %entry], [%i.inc, %loop]
  %val.loop = phi i8 [%val.i8, %entry], [%val.inc, %loop]
  call void @llvm.memset.p0i8.p0i8.i32(i8* %ptr.src, i8 %val.loop, i32 15, i1 false)
  call void @llvm.memset.p0i8.p0i8.i32(i8* %ptr.dst, i8 0, i32 15, i1 false)
  ; We add a volatile load (whose value is unused) to ensure optimizations
  ; don't remove either llvm.memcpy or llvm.memset
  %barrier = load volatile i8, i8* %ptr.dst
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr.dst, i8* %ptr.src, i32 15, i1 false)
  %i.inc = add i8 %i, 1
  %val.inc = add i8 %val.loop, 1
  %repeat = icmp ult i8 %i.inc, %val.i8
  br i1 %repeat, label %loop, label %end, !llvm.loop !0

end:
  %ptr.end = getelementptr i8, i8* %ptr.dst, i8 %val.i8
  %result = load volatile i8, i8* %ptr.end
  %result.i32 = zext i8 %result to i32
  ret i32 %result.i32
}
declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)
declare void @llvm.memset.p0i8.p0i8.i32(i8*, i8, i32, i1)

!0 = !{!0, !1}
!1 = !{!"llvm.loop.bound", i32 0, i32 15}