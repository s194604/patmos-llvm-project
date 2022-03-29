// RUN: %clang_cc1 -triple=patmos %s -emit-llvm -o - | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Test that when the loop bound pragram is given, it is propagated into the LLVM-IR
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-LABEL: func1
int func1(int x) { 
	int count = x;
	
	_Pragma( "loopbound min 3 max 7" )
	while(x >0){
	// CHECK: br i1
	// CHECK-SAME: !llvm.loop ![[LOOP_1:[0-9]+]]
		x = x/2;
		count++;
	}
	return count;	
}

// CHECK-LABEL: func2
int func2(int x) { 
	int count = x;
	
	_Pragma( "loopbound min 24 max 92658" )
	for(int i = 0; i < x; i++){
	// CHECK: br i1
	// CHECK-SAME: !llvm.loop ![[LOOP_2:[0-9]+]]
		x = x/2;
		count++;
	}
	return count;	
}

// CHECK-LABEL: func3
int func3(int x) { 
	int count = x;
	
	#pragma loopbound min 0 max 124
	do {
	// CHECK: br i1
	// CHECK-SAME: !llvm.loop ![[LOOP_3:[0-9]+]]
		x = x/2;
		count++;
	} while(x >0);
	return count;	
}

// CHECK-DAG: ![[LOOP_1]] = distinct !{![[LOOP_1]], ![[LOOP_12:[0-9]+]]}
// CHECK-DAG: ![[LOOP_12]] = !{!"llvm.loop.bound", i32 3, i32 7}
// CHECK-DAG: ![[LOOP_2]] = distinct !{![[LOOP_2]], ![[LOOP_22:[0-9]+]]}
// CHECK-DAG: ![[LOOP_22]] = !{!"llvm.loop.bound", i32 24, i32 92658}
// CHECK-DAG: ![[LOOP_3]] = distinct !{![[LOOP_3]], ![[LOOP_32:[0-9]+]]}
// CHECK-DAG: ![[LOOP_32]] = !{!"llvm.loop.bound", i32 0, i32 124}