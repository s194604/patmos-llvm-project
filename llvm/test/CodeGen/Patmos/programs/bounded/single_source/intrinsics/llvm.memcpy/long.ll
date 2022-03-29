;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without needing the standard library "memset" for large values
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
; RUN: MEMCPY_DEST_PTR_INC="0"
; RUN: MEMCPY_SRC_PTR_INC="0"

; RUN: EXEC_ARGS="0=0 1=1 2=2 6=6"
; RUN: MEMCPY_ALLOC_COUNT="513"
; RUN: MEMCPY_COUNT="513"
; RUN: %memcpy_check_i32 %test_execution
; RUN: %memcpy_check_i64 %test_execution
