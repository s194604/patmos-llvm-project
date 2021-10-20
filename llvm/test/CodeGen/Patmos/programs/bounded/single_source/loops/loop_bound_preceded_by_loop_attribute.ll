; RUN: EXEC_ARGS="0=0 1=2 2=5 3=9 5=20"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests that a loop bound attribute may be grouped and preceded by another
; loop attribute (specifically, an attribute whose name also starts with "llvm.loop").
;
; The following is the equivalent C code:
; volatile int _2 = 2;
; 
; int main(int iteration_count){
; 	int x = 0;
;   #pragma clang loop unroll_count(2)
; 	#pragma loopbound min 0 max 4
; 	for(int i = 0; i<iteration_count; i++){
; 		x += i + _2;
; 	}
; 	return x;
; }
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_2 = global i32 2

define i32 @main(i32 %iteration_count)  {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %x.0 = phi i32 [ 0, %entry ], [ %add1, %for.inc ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %cmp = icmp slt i32 %i.0, %iteration_count
  br i1 %cmp, label %for.body, label %for.end, !llvm.loop !0

for.body:                                         ; preds = %for.cond
  %0 = load volatile i32, i32* @_2
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %add = add nsw i32 %i.0, %0
  %add1 = add nsw i32 %x.0, %add
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret i32 %x.0
}

!0 = !{!0, !1, !2}
!1 = !{!"llvm.loop.unroll.count", i32 2}
!2 = !{!"llvm.loop.bound", i32 0, i32 4}
