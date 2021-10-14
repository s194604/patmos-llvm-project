// RUN: %test-patmos-librt
// END.

// a / b, *rem = a % b
extern unsigned int __udivmodsi4(unsigned int a, unsigned int b, unsigned int* rem);

int main()
{
	#define nr_test_cases 5
	
	// {input_a,input_b,expected_return,expected_rem}
	unsigned int test_cases[nr_test_cases][4] = { {0,1,0,0}, {1,1,1,0}, {1,2,0,1}, {2,1,2,0}, {123,24,5,3} };
	
	for(int i = 0; i<nr_test_cases; i++) {
		unsigned int rem = 0;
		if(__udivmodsi4(test_cases[i][0], test_cases[i][1], &rem) != test_cases[i][2] || rem != test_cases[i][3]) {
			return i+1;
		}
	}		
	return 0;
}
