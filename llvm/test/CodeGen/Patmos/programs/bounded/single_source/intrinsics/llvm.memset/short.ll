;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can use llvm.memset without needing the standard library "memset" for small values
;
;//////////////////////////////////////////////////////////////////////////////////////////////////
; RUN: MEMSET_ALLOC_COUNT="10"
; RUN: MEMSET_PTR_INC="0"

; RUN: EXEC_ARGS="0=0 1=1 2=2 100=100"
; RUN: MEMSET_COUNT="1"
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=2 2=4 60=120"
; RUN: MEMSET_COUNT="2"
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=3 2=6 60=180"
; RUN: MEMSET_COUNT="3"
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution

; RUN: EXEC_ARGS="0=0 1=4 2=8 60=240"
; RUN: MEMSET_COUNT="4"
; RUN: %memset_check_i32 %test_execution
; RUN: %memset_check_i64 %test_execution
