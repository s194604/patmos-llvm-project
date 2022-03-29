# Make the directory that the mock std libs will be installed in
set(PATMOS_LIB_DIR "${LLVM_BINARY_DIR}/patmos-unknown-unknown-elf/lib")
add_custom_target(ClangPatmosMockStdLibsDirs
    "${CMAKE_COMMAND}" -E make_directory ${PATMOS_LIB_DIR}
)

# Helper for compile command
set(COMPILE ${LLVM_TOOLS_BINARY_DIR}/clang --target=patmos -c -emit-llvm)

# Single-source libs
set(SS_LIBS_SOURCES "crt0" "crtbegin" "crtend")
set(SS_LIBS_O)
foreach(p ${SS_LIBS_SOURCES})
	# Compile
	add_custom_command( 
		OUTPUT ${PATMOS_LIB_DIR}/${p}.o
		COMMAND  ${COMPILE} ${p}.c -o ${PATMOS_LIB_DIR}/${p}.o
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ClangPatmosMockStdLibsDirs clang 
			${CMAKE_CURRENT_SOURCE_DIR}/${p}.c 
		)
	list(APPEND SS_LIBS_O ${PATMOS_LIB_DIR}/${p}.o)
endforeach()

# Multi-source libs
set(MS_LIBS "librt" "libc" "libpatmos" "libm")
set(MS_LIBS_A)
foreach(p ${MS_LIBS})
	# Compile
	add_custom_command( 
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${p}.o
		COMMAND  ${COMPILE} ${p}.c -o ${CMAKE_CURRENT_BINARY_DIR}/${p}.o
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ClangPatmosMockStdLibsDirs clang 
			${CMAKE_CURRENT_SOURCE_DIR}/${p}.c 
		)
	# Build archive
	add_custom_command( 
		OUTPUT ${PATMOS_LIB_DIR}/${p}.a
		COMMAND  ar cr ${PATMOS_LIB_DIR}/${p}.a  ${CMAKE_CURRENT_BINARY_DIR}/${p}.o
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ClangPatmosMockStdLibsDirs clang 
			${CMAKE_CURRENT_BINARY_DIR}/${p}.o 
		)
	list(APPEND MS_LIBS_A ${PATMOS_LIB_DIR}/${p}.a)
endforeach()

# Copy the std public API list file
set( SYMS_FILE ${PATMOS_LIB_DIR}/libsyms.lst)
add_custom_command( 
	OUTPUT ${SYMS_FILE}
	COMMAND cp libsyms.lst ${SYMS_FILE}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS ClangPatmosMockStdLibsDirs ${CMAKE_CURRENT_SOURCE_DIR}/libsyms.lst
	)

add_custom_target(ClangPatmosMockStdLibs DEPENDS ${SS_LIBS_O} ${MS_LIBS_A} ${SYMS_FILE})
add_dependencies(ClangPatmosTestDeps ClangPatmosMockStdLibs)