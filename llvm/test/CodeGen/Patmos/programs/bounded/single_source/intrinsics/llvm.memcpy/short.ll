;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memcpy without needing the standard library "memcpy" for small values
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
; RUN: MEMCPY_ALLOC_COUNT="10"
; RUN: MEMCPY_DEST_PTR_INC="0"
; RUN: MEMCPY_SRC_PTR_INC="0"

; RUN: EXEC_ARGS="0=0 1=1 2=2 100=100"
; RUN: MEMCPY_COUNT="1"
; RUN: %memcpy_check_i32 %test_execution
; RUN: %memcpy_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=2 2=4 60=120"
; RUN: MEMCPY_COUNT="2"
; RUN: %memcpy_check_i32 %test_execution
; RUN: %memcpy_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=3 2=6 60=180"
; RUN: MEMCPY_COUNT="3"
; RUN: %memcpy_check_i32 %test_execution
; RUN: %memcpy_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=4 2=8 60=240"
; RUN: MEMCPY_COUNT="4"
; RUN: %memcpy_check_i32 %test_execution
; RUN: %memcpy_check_i64 %test_execution
