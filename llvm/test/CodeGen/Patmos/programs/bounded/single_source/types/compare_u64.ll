; RUN: EXEC_ARGS="1=0 9=0 10=0 100=1"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests can compare two i64 values
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_10 = global i64 10

define i32 @main(i32 %x) {
entry:
  %retval = alloca i32
  %x.addr = alloca i32
  store i32 0, i32* %retval
  store i32 %x, i32* %x.addr
  %0 = load i32, i32* %x.addr
  %conv = sext i32 %0 to i64
  %1 = load volatile i64, i64* @_10
  %cmp = icmp ugt i64 %conv, %1
  %conv1 = zext i1 %cmp to i32
  ret i32 %conv1
}
