declare i32 @main()

; We use inline assembly to have full control of how the program execution starts
module asm "
		.word 76 # I-Cache fetch for _start
		.text
		.globl _start
		.type _start @function
	_start:
		li 		$r31 	= 2064384	# initialize shadow stack pointer (0x1f8000)
		li		$r1		= 2097152	# (0x200000)
		mts 	$ss  	= $r1		# initialize the stack cache's spill pointer 
		mts 	$st  	= $r1		# initialize the stack cache's top pointer
		li		$r1		= _end
		mts		$srb	= $r1		# Set the return point for main such that it returns to _end
		mts		$sro	= $r0
		li		$r1		= main
		brcfnd 	$r1
		
		.word 20		# I-Cache fetch for _end
	_end:
		.word 88080384	# this is a magic instruction that pasim interprets as 'halt'
		nop
		nop
		nop
		nop
"
