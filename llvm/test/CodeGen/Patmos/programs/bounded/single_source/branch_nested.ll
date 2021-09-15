; RUN: EXEC_ARGS="0=125 1=128 2=132 3=133 6=136 7=137 8=138"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests nested branching. 
; The following is the equivalent C code:
; 
; volatile int cond = 0;
;  
; int init_func(){
; 	int x = cond + 128;
; 	if(cond<2){
; 		if(cond<1){
;			x -= 1;
; 		}else{
; 			x += 1;
; 		}
; 		x -=2;
; 	}else{
; 		x += 2;
; 	}
; 	return x;
; }
;  
; int main(int x){
;   cond = x;
;   return init_func();
; }
;//////////////////////////////////////////////////////////////////////////////////////////////////

@cond = global i32 0

define i32 @init_func()  {
entry:
  %cond = load volatile i32, i32* @cond
  %x = add nsw i32 %cond, 128
  %cmp = icmp slt i32 %cond, 2
  br i1 %cmp, label %if.then, label %if.else

if.then:
  %cmp2 = icmp slt i32 %cond, 1
  br i1 %cmp2, label %if.then2, label %if.else2

if.then2:
  %xm1 = add nsw i32 %x, -1
  br label %if.then.end
  
if.else2:
  %xp1 = add nsw i32 %x, 1
  br label %if.then.end
  
if.then.end:
  %xx1 = phi i32 [ %xm1, %if.then2 ], [ %xp1, %if.else2 ]
  %xx1m2 = add nsw i32 %xx1, -2
  br label %end

if.else:
  %xp2 = add nsw i32 %x, 2
  br label %end

end:                                        
  %xf = phi i32 [ %xp2, %if.else ], [ %xx1m2, %if.then.end ]
  ret i32 %xf
}

define i32 @main(i32 %x)  {
entry: 
  store volatile i32 %x, i32* @cond
  %call1 = call i32 @init_func()
  ret i32 %call1
}
