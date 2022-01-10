;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; This is not a test file. It is used by other test files to run their tests.
;
; Creates a test of the intrinsic llvm.memcpy to ensure that it will result
; in code that works without the need to the standar library 'memcpy' function.
;
; User tests must substitute all occurances of the following strings:
;	<type>: 
;		The type of the length argument to llvm.memcpy
;	<count>: 
;		The constant value of the length argument to llvm.menset
;	<alloc_count>:
;		The constant value of how many bytes should be allocated before the test starts.
;		The allocated memory is overwritten by llvm.memcpy during the test.
;		The initial allocated memory pointer is always 4-aligned.
;	<ptr_inc>:
;		How much to increment the pointer returned by the allocation.
;		Use 0 for no incrementation
;	<ptr_attr>:
;		Attributes on the llvm.memcpy destination pointer.
;
; The test sets each byte to the value given as an argument to main.
; After memcpy has run, it loads all the set bytes and sums their values.
; the sum is then returned.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main(i32 %set_to)  {
entry:
  %ptr.src.0 = alloca i8, i32 <alloc_count>, align 4
  %ptr.dest.0 = alloca i8, i32 <alloc_count>, align 4
  %set_to_i8 = trunc i32 %set_to to i8
  ; Set the source values
  call void @llvm.memset.p0i8.i32(i8* noundef nonnull align 4 %ptr.src.0, i8 %set_to_i8, i32 <alloc_count>, i1 false)
  ; Clear destination values
  call void @llvm.memset.p0i8.i32(i8* noundef nonnull align 4 %ptr.dest.0, i8 0, i32 <alloc_count>, i1 false)
  %ptr.src = getelementptr i8, i8* %ptr.src.0, i32 <src_ptr_inc>
  %ptr.dest = getelementptr i8, i8* %ptr.dest.0, i32 <dest_ptr_inc>
  call void @llvm.memcpy.p0i8.p0i8.<type>(i8* <dest_ptr_attr> %ptr.dest, i8* <src_ptr_attr> %ptr.src, <type> <count>, i1 false)
  br label %for.cond

for.cond:
  %i = phi i32 [<count>, %entry], [%i.decd, %for.body]
  %dest = phi i8* [%ptr.dest, %entry], [%dest.incd, %for.body]
  %sum = phi i8 [0, %entry], [%sum.next, %for.body]
  %stop = icmp eq i32 %i, 0
  br i1 %stop, label %for.end, label %for.body, !llvm.loop !0

for.body:
  %read = load volatile i8, i8* %dest
  %sum.next = add i8 %sum, %read
  %i.decd = sub i32 %i, 1
  %dest.incd = getelementptr i8, i8* %dest, i32 1
  br label %for.cond
  
for.end:
  %result = zext i8 %sum to i32
  ret i32 %result
}

declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i1)
declare void @llvm.memcpy.p0i8.p0i8.<type>(i8*, i8*, <type>, i1)

!0 = !{!0, !1}
!1 = !{!"llvm.loop.bound", i32 <count>, i32 <count>}