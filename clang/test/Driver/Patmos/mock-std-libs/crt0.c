///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Mock lib for crt0.o
//
///////////////////////////////////////////////////////////////////////////////////////////////////

volatile int PATMOS_MOCK_CRT0_INT = 1;

extern int main();
extern int patmos_mock_crtbegin_function(int);
extern int patmos_mock_crtend_function(int);

// Use main, crtbegin, crtend such that they are never optimized away
// during testing.
void _start() __attribute__((used));
void _start() {
	patmos_mock_crtend_function(patmos_mock_crtbegin_function(main()));
}

int patmos_mock_crt0_function(int x)  {
	
	return x + PATMOS_MOCK_CRT0_INT;
}
