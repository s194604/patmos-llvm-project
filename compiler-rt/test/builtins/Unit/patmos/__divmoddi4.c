// RUN: %test-patmos-librt
// END.

// a / b, *rem = a % b
extern signed long long __divmoddi4(signed long long a, signed long long b, signed long long * rem);

int main()
{
	#define nr_test_cases 7
	
	// {input_a,input_b,expected_return,expected_rem}
	signed long long test_cases[nr_test_cases][4] = { {0,1,0,0}, {1,1,1,0}, {1,2,0,1}, {2,1,2,0}, {123,24,5,3}, {-1,1,-1,0}, {-1,-2,0,-1} };
	
	for(int i = 0; i<nr_test_cases; i++) {
		signed long long rem = 0;
		if(__divmoddi4(test_cases[i][0], test_cases[i][1], &rem) != test_cases[i][2] || rem != test_cases[i][3]) {
			return i+1;
		}
	}		
	return 0;
}
