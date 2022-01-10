## Memset Tests

These tests ensure the `llvm.memset` will be converted to inline code and not need the `memset` standard library function.

`check_set.ll` contains the code for each individual test, however needs some substitutions to be complete.

`%memset_check_i32` and `%memset_check_i64` are prefixes to `%test_execution` that use `check_set.ll` instead of the calling test file, substitute where needed and run the test.
You must use both, to ensure that both the `i32` and `i64` versions of `llvm.memset` are tested.
The following variables must be set before calling the tests:

* `MEMSET_COUNT`: The length argument to `llvm.memset`
* `MEMSET_ALLOC_COUNT`: How much space (in bytes) the test should allocate. This may be larger than `MEMSET_COUNT`.
* `MEMSET_PTR_INC`: By how many bytes to increment the destination pointer before passing it to `llvm.memset`. 
	The destination pointer by default is 4-aligned. 
	This allows you to change that alignment by increment. 
	If no increment is needed, use `0`.
* `MEMSET_PTR_ATTR`: Attributes to add to the destination pointer when passed to `llvm.memset`. Optional.
