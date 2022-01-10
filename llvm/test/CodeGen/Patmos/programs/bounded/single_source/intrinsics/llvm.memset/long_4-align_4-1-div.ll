; RUN: EXEC_ARGS="0=0 1=43 2=86 5=215" 
; RUN: MEMSET_COUNT="43"
; RUN: MEMSET_ALLOC_COUNT="43"
; RUN: MEMSET_PTR_INC="0" 
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without standard library 'memset' for:
; * High length
; * 4-Aligned start
; * 4-divisable length minus 1
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
