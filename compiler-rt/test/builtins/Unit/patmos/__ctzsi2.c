// RUN: %test-patmos-librt
// END.

// Trailing 0s
extern int __ctzsi2(unsigned int a);

int main()
{
	#define nr_test_cases 5
	
	// Each test has first the test data then the expected result
	unsigned int test_cases[nr_test_cases][2] = { {1,0}, {2,1}, {3, 0}, {4, 2}, {5, 0} };
	
	for(int i = 0; i<nr_test_cases; i++) {
		if(__ctzsi2(test_cases[i][0]) != test_cases[i][1]) {
			return i+1;
		}
	}		
	return 0;
}
