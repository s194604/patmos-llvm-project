; RUN: llc < %s -mpatmos-subfunction-align=8 --align-all-blocks=4 \
; RUN: -mpatmos-max-subfunction-size=80 | FileCheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that when both --align-all-blocks and -mpatmos-subfunction-align are used,
; -mpatmos-subfunction-align takes precedence for blocks starting a subfunction.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_7 = global i32 7

; CHECK: .p2align 3
; CHECK: .fstart	main, .Ltmp{{[0-9]}}-main, 8
; CHECK: main:
define i32 @main()  {
entry:
  %0 = load volatile i32, i32* @_7
  %tobool = icmp eq i32 %0, 7
  br i1 %tobool, label %if.else, label %if.then

if.else:                                          ; preds = %entry
  %1 = sub nsw i32 %0, 7
  br label %if.end
 
if.then:                                          ; preds = %entry
  %2 = add nsw i32 %0, 2
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %result = phi i32 [ %2, %if.then ], [ %1, %if.else ]
  ret i32 %result
}

; CHECK: .p2align 3
; CHECK: .fstart	main2, .Ltmp{{[0-9]}}-main2, 8
; CHECK: main2:
define i32 @main2()  {
entry:
  %0 = load volatile i32, i32* @_7
  %tobool = icmp eq i32 %0, 7
  br i1 %tobool, label %if.else, label %if.then

; CHECK: .p2align 3
; CHECK: .fstart	[[ELSE_BLOCK:.LBB1_[0-9]]], .Ltmp{{[0-9]}}-[[ELSE_BLOCK]], 8
; CHECK: [[ELSE_BLOCK]]:
if.else:                                          ; preds = %entry
  %1 = sub nsw i32 %0, 7
  %2 = load volatile i32, i32* @_7
  %3 = load volatile i32, i32* @_7
  %4 = load volatile i32, i32* @_7
  %5 = load volatile i32, i32* @_7
  %6 = load volatile i32, i32* @_7
  %7 = load volatile i32, i32* @_7
  br label %if.end
 
; CHECK: .p2align 3
; CHECK: .fstart	[[THEN_BLOCK:.LBB1_[0-9]]], .Ltmp{{[0-9]}}-[[THEN_BLOCK]], 8
; CHECK: [[THEN_BLOCK]]:
if.then:                                          ; preds = %entry
  %8 = add nsw i32 %0, 2
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %result = phi i32 [ %8, %if.then ], [ %1, %if.else ]
  ret i32 %result
}