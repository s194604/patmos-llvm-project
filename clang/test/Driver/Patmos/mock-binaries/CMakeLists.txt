
# For testing what flags patmos-ld gets when linking full programs
add_executable(mock-patmos-ld patmos-ld.c)
set_target_properties(mock-patmos-ld PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)

# For testing that a binary isn't called
add_executable(mock-patmos-illegal illegal.c)
set_target_properties(mock-patmos-illegal PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	
add_dependencies(ClangPatmosTestDeps mock-patmos-ld mock-patmos-illegal)