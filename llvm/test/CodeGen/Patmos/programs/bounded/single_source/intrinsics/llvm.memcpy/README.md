## Memcpy Tests

These tests ensure the `llvm.memcpy` will be converted to inline code and not need the `memcpy` standard library function.

`check_cpy.ll` contains the code for each individual test, however needs some substitutions to be complete.

`%memcpy_check_i32` and `%memcpy_check_i64` are prefixes to `%test_execution` that use `check_cpy.ll` instead of the calling test file, substitute where needed and run the test.
You must use both, to ensure that both the `i32` and `i64` versions of `llvm.memcpy` are tested.
The following variables must be set before calling the tests:

* `MEMCPY_COUNT`: The length argument to `llvm.memcpy`
* `MEMCPY_ALLOC_COUNT`: How much space (in bytes) the test should allocate. This may be larger than `MEMCPY_COUNT`.
* `MEMCPY_DEST_PTR_INC`: By how many bytes to increment the destination pointer before passing it to `llvm.memcpy`. 
	The destination pointer by default is 4-aligned. 
	This allows you to change that alignment by increment. 
	If no increment is needed, use `0`.
* `MEMCPY_SRC_PTR_INC`: Like the previous but for the source pointer.
* `MEMCPY_DEST_PTR_ATTR`: Attributes to add to the destination pointer when passed to `llvm.memcpy`. Optional.
* `MEMCPY_SRC_PTR_ATTR`: Like the previous but for the source pointer. Optional.
