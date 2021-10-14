// RUN: %test-patmos-librt
// END.

// a / b
extern unsigned int __udivsi3(unsigned int n, unsigned int d);               

int main()
{
	#define nr_test_cases 5
	
	// {input_a,input_b,expected_return}
	unsigned int test_cases[nr_test_cases][3] = { {0,1,0}, {1,1,1}, {1,2,0}, {2,1,2}, {123,24,5} };
	
	for(int i = 0; i<nr_test_cases; i++) {
		if(__udivsi3(test_cases[i][0], test_cases[i][1]) != test_cases[i][2]) {
			return i+1;
		}
	}		
	return 0;
}
