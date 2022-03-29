; RUN: EXEC_ARGS="0=0 1=20 2=40 6=120" 
; RUN: MEMSET_COUNT="20"
; RUN: MEMSET_ALLOC_COUNT="20"
; RUN: MEMSET_PTR_INC="0" 
; RUN: MEMSET_PTR_ATTR="noundef nonnull align 4" 
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that memset accepts destination pointer with explicit 4-alignment.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; RUN: llc < %s | FileCheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that memset will use word-stores if the destination is word-aligned
;
; We use a length that is divisable by 4 such that no extra halfword or byte store are needed
; we can therefore test that they aren't used.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@ptr = external global i8, align 4

; CHECK-LABEL: main:
define i8 @main(){
entry:
  %ptr2 = getelementptr i8, i8* @ptr, i32 0
  call void @llvm.memset.p0i8.i32(i8* noundef nonnull align 4 %ptr2, i8 3, i32 40, i1 false)
  %read = load volatile i8, i8* @ptr
  ret i8 %read
}

; We check that only word-stores are used

; First, check that a 4-byte version of our value is loaded in a register
; CHECK: li $[[REG:r[0-9]]] = 50529027
; Then check that the register is stored using word-type store instruction
; CHECK: {{sw[mcsl]}} [${{r[0-9]}}] = $[[REG]]

declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i1)