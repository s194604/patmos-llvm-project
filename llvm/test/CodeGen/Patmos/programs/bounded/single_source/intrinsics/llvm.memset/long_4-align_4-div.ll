; RUN: EXEC_ARGS="0=0 1=40 2=80 6=240" 
; RUN: MEMSET_COUNT="40"
; RUN: MEMSET_ALLOC_COUNT="40"
; RUN: MEMSET_PTR_INC="0" 
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without standard library 'memset' for:
; * High length
; * 4-Aligned start
; * 4-divisable length
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
