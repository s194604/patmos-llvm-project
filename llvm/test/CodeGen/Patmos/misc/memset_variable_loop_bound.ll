; RUN: llc %s -o %t
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure variable-length llvm.memset calls are allowed in non-singlepath code.
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="no_memset" -o %t
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if single-path is enabled, but a variable-length llvm.memset is not in one of the 
; singlepath functions, no error is thrown
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="with_memset" %XFAIL-filecheck %s
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if a single-path function contains a call to llvm.memset with a variable length argument,
; an error is thrown.
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc %s -mpatmos-singlepath="main" %XFAIL-filecheck %s
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure if a single-path function (which isn't the single-path-root) contains a call to 
; llvm.memset with a variable length argument, an error is thrown.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; END

; CHECK: llvm.memset length argument not a constant value

define i32 @main(i32 %x, i8* %ptr) {
entry:
  call void @no_memset(i32 %x, i8* %ptr)
  call void @with_memset(i32 %x, i8* %ptr)
  ret i32 0
}

define void @no_memset(i32 %x, i8* %ptr) {
entry:
  store i8 4, i8* %ptr
  ret void
}

define void @with_memset(i32 %x, i8* %ptr) {
entry:
  call void @llvm.memset.p0i8.i32(i8* %ptr, i8 9, i32 %x, i1 false)
  ret void
}

declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i1)


