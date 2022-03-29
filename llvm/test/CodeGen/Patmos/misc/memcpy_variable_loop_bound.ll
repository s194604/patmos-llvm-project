; RUN: llc %s -o %t
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure variable-length llvm.memcpy calls are allowed in non-singlepath code.
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="no_memcpy" -o %t
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if single-path is enabled, but a variable-length llvm.memcpy is not in one of the 
; singlepath functions, no error is thrown
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="with_memcpy" %XFAIL-filecheck %s
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if a single-path function contains a call to llvm.memcpy with a variable length argument,
; an error is thrown.
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="main" %XFAIL-filecheck %s
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if a single-path function (which isn't the single-path-root) contains a call to 
; llvm.memcpy with a variable length argument, an error is thrown.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; END

; CHECK: llvm.memcpy length argument not a constant value

define i32 @main(i32 %x, i8* %ptr.dst, i8* %ptr.src) {
entry:
  call void @no_memcpy(i32 %x, i8* %ptr.dst, i8* %ptr.src)
  call void @with_memcpy(i32 %x, i8* %ptr.dst, i8* %ptr.src)
  ret i32 0
}

define void @no_memcpy(i32 %x, i8* %ptr.dst, i8* %ptr.src) {
entry:
  store i8 4, i8* %ptr.dst
  ret void
}

define void @with_memcpy(i32 %x, i8* %ptr.dst, i8* %ptr.src) {
entry:
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr.dst, i8* %ptr.src, i32 %x, i1 false)
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)


