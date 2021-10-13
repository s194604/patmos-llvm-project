///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Mock patmos-ld. Used to verify that clang calls patmos-ld correctly.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include<string.h>

int check(int argc, char *argv[], char *expected) {
	int found_crt0 = 0;
	for(int i = 0; i < argc; i++){
		if ( strcmp(argv[i],expected) == 0) {
			found_crt0 = 1;
			break;
		}
	}
	if (!found_crt0) {
		fprintf( stderr, "Error: patmos-ld expected argument: '%s'\n", expected);
	}
	return !found_crt0;
}

int main(int argc, char *argv[])  {
	int ret_code = 0;
	
	ret_code += check(argc, argv, "-nostdlib");
	ret_code += check(argc, argv, "-static");
	ret_code += check(argc, argv, "-o");
	ret_code += check(argc, argv, "lib/crt0.o");
	ret_code += check(argc, argv, "lib/crtbegin.o");
	ret_code += check(argc, argv, "lib/crtend.o");
	ret_code += check(argc, argv, "-lc");
	ret_code += check(argc, argv, "-lpatmos");
	ret_code += check(argc, argv, "-lrtsf");
	ret_code += check(argc, argv, "-lrt");
	
	if (ret_code > 0 ) {
		fprintf( stderr, "Arguments: ");
		for(int i = 1; i < argc; i++){
			fprintf( stderr, "%s ", argv[i]);
		}
		fprintf( stderr, "\n");
	}
	return ret_code;
}

