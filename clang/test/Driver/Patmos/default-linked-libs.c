// RUN: %clang --target=patmos %s -o %t
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that the correct libs are linked by default
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Declare the functions in the mock libs
extern int patmos_mock_crt0_function(int);
extern int patmos_mock_libc_function(int);
extern int patmos_mock_libm_function(int);
extern int patmos_mock_libpatmos_function(int);
extern int patmos_mock_librt_function(int);

int main() { 
	return 
		patmos_mock_crt0_function(
		patmos_mock_librt_function(
		patmos_mock_libpatmos_function(
		patmos_mock_libc_function(
		patmos_mock_libm_function(
		0)))));
}