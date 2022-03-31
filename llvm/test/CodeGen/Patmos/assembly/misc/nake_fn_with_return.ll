; RUN: %test_no_runtime_execution

; Tests can have a naked function with a parameter that is used by the assembly

define i32 @naked_fn() #0 {
entry:
  %0 = tail call i32 asm sideeffect "
    li $0 = 3
    ", "=r"()
  ret i32 %0
}
attributes #0 = { naked noinline }

define i32 @main() {
entry:
  %0 = call i32 @naked_fn()
  %1 = sub i32 %0, 3
  ret i32 %1
}
