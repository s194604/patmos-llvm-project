// RUN: %test-patmos-librt
// END.

// Leading 0s
extern int __clzsi2(unsigned int a);

int main()
{
	#define nr_test_cases 3
	
	// Each test has first the test data then the expected result
	unsigned int test_cases[nr_test_cases][2] = { {0,32}, {1,31}, {2, 30} };
	
	for(int i = 0; i<nr_test_cases; i++) {
		if(__clzsi2(test_cases[i][0]) != test_cases[i][1]) {
			return i+1;
		}
	}		
	return 0;
}
