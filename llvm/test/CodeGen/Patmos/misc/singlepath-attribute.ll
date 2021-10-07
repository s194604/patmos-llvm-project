; RUN: llc %s -mpatmos-singlepath="" -o - | FileCheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Tests that if "sp-root" is given as an attribute, the function will be 
; compiled using single-path code.
;
; To check for single-path. We ensure no branches are produced, 
; and the key instructions are predicated
;
; The following is the equivalent C code:
;
; int some_fn(int x) {
; 	if( x > 0) {
; 		return x*x;
; 	} else {
; 		return x+3;
; 	}
; }
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @some_fn(i32 %x) #0 {
entry:
  %retval = alloca i32
  %x.addr = alloca i32
  store i32 %x, i32* %x.addr
  %0 = load i32, i32* %x.addr
  %cmp = icmp sgt i32 %0, 0
  
  ; CHECK-NOT: br
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %1 = load i32, i32* %x.addr
  %2 = load i32, i32* %x.addr
  
  ; CHECK-DAG:  mul ( $p{{[1-7]}})
  %mul = mul nsw i32 %1, %2
  store i32 %mul, i32* %retval
  br label %return

if.else:                                          ; preds = %entry
  %3 = load i32, i32* %x.addr
  
  ; CHECK-DAG:  add ( $p{{[1-7]}})
  %add = add nsw i32 %3, 3
  store i32 %add, i32* %retval
  br label %return

return:                                           ; preds = %if.else, %if.then
  %4 = load i32, i32* %retval
  ret i32 %4
}

attributes #0 = { noinline "sp-root" }

