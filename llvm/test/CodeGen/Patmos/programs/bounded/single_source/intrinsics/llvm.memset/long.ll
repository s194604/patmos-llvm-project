;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without needing the standard library "memset" for large values
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
; RUN: MEMSET_PTR_INC="0"

; RUN: EXEC_ARGS="0=0 1=1 2=2 6=6"
; RUN: MEMSET_ALLOC_COUNT="513"
; RUN: MEMSET_COUNT="513"
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
