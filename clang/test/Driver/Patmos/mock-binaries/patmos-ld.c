///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Mock patmos-ld. Used to verify that clang calls patmos-ld correctly.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Checks that argc/argv include the given list of strings (in sequence)
int check(int argc, char *argv[], int n_expected, ...) {
	int found = 0;
	
	// Initialize list of expected args
    va_list expected_list;
    va_start(expected_list, n_expected);
	char* first_expected = va_arg(expected_list, char*);
	
	// Ignore first argument as its always the program path.
	for(int i = 1; i < argc; i++){
		if ( strcmp(argv[i],first_expected) == 0) {
			found = 1;
			// Found the first, ensure all following also present immediately after
			for(int j = 1; j < n_expected; j++) {
				char* next_expected = va_arg(expected_list, char*);
				int next_arg = i+j;
				if ( next_arg >= argc || strcmp(argv[next_arg],next_expected) != 0) {
					// Next expected didn't match next arg
					found = 0;
					break;
				}
			}
			if(found) {
				break;
			} else {
				// Have to continue looking, so must reset expected_list
				va_end(expected_list);
				va_start(expected_list, n_expected);
				first_expected = va_arg(expected_list, char*);				
			}
		}
	}
	va_end(expected_list);
	
	if (!found) {
		fprintf( stderr, "Error: patmos-ld expected arguments: ");
		
		va_start(expected_list, n_expected);
		for(int i = 0; i < n_expected; i++){
			fprintf( stderr, "%s ", va_arg(expected_list, char*));
		}
		va_end(expected_list);
		
		fprintf( stderr, "\n");
	}
	return !found;
}

int main(int argc, char *argv[])  {
	int ret_code = 0;
	
	ret_code += check(argc, argv, 1, "-nostdlib");
	ret_code += check(argc, argv, 1, "-static");
	ret_code += check(argc, argv, 1, "-o");
	ret_code += check(argc, argv, 2, "--defsym", "__heap_start=end");
	ret_code += check(argc, argv, 2, "--defsym", "__heap_end=0x100000");
	ret_code += check(argc, argv, 2, "--defsym", "_shadow_stack_base=0x1f8000");
	ret_code += check(argc, argv, 2, "--defsym", "_stack_cache_base=0x200000");
	
	if (ret_code > 0 ) {
		fprintf( stderr, "Arguments: ");
		for(int i = 1; i < argc; i++){
			fprintf( stderr, "%s ", argv[i]);
		}
		fprintf( stderr, "\n");
	}
	return ret_code;
}

