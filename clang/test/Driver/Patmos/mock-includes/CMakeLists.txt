# Make the directory that the mock std headers will be installed in
set(PATMOS_INCLUDE_DIR "${LLVM_BINARY_DIR}/patmos-unknown-unknown-elf/include")
add_custom_target(ClangPatmosMockStdHeadersDirs
    "${CMAKE_COMMAND}" -E make_directory ${PATMOS_INCLUDE_DIR}
)

# Copy mock std header
add_custom_command( 
	OUTPUT ${PATMOS_INCLUDE_DIR}/std-test-header.h
	COMMAND cp std-test-header.h ${PATMOS_INCLUDE_DIR}/std-test-header.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ClangPatmosMockStdHeadersDirs 
		${CMAKE_CURRENT_SOURCE_DIR}/std-test-header.h 
	)

add_custom_target(ClangPatmosMockStdHeaders DEPENDS ${PATMOS_INCLUDE_DIR}/std-test-header.h )
add_dependencies(ClangPatmosTestDeps ClangPatmosMockStdHeaders)