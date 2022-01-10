; RUN: EXEC_ARGS="0=0 1=51 2=102 5=255"
; RUN: MEMSET_COUNT="51"
; RUN: MEMSET_ALLOC_COUNT="52"
; RUN: MEMSET_PTR_INC="1" 
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
; END
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without standard library 'memset' for:
; * High length
; * 1-Aligned start
; * 4-divisable length minus 1
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
