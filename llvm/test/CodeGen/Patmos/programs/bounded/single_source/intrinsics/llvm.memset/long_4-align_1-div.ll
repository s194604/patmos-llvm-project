; RUN: EXEC_ARGS="0=0 1=41 2=82 6=246"
; RUN: MEMSET_COUNT="41"
; RUN: MEMSET_ALLOC_COUNT="41"
; RUN: MEMSET_PTR_INC="0" 
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without standard library 'memset' for:
; * High length
; * 4-Aligned start
; * 1-divisable length
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
