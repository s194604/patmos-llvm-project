
# Save the target triple in a variable
execute_process( COMMAND gcc -dumpmachine OUTPUT_VARIABLE TARGET_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE )
set( PROJECT_NAME "patmos-llvm")
set( PATMOS_TRIPLE "patmos-unknown-unknown-elf" )
set( PACKAGE_TEMP_DIR "${PATMOS_TRIPLE}/package-temp")
set( PACKAGE_TAR "${PACKAGE_TEMP_DIR}/${PROJECT_NAME}-${TARGET_TRIPLE}.tar")
set( PACKAGE_TAR_GZ "${PACKAGE_TAR}.gz")
set( PACKAGE_INFO_FILE "${PACKAGE_TEMP_DIR}/${PROJECT_NAME}-info.yml")
set( PACKAGE_TARGETS "llc" "llvm-link" "clang" "llvm-config" "llvm-objdump" "opt")
set( PACKAGE_ITEMS_LIBS
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/crt0.o"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/crtbegin.o"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/crtend.o"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/libc.a"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/libm.a"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/libpatmos.a"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/librt.a"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/libsyms.lst"
)
set( NEWLIB_INCLUDES # List of dirs from which to get only the files
	"newlib/libc/include"
	"newlib/libc/include/machine"
	"newlib/libc/include/rpc"
	"newlib/libc/include/sys"
	"newlib/libc/machine/patmos/machine" # -> include/machine
	"newlib/libc/machine/patmos/sys" # -> include/sys
	"newlib/libc/machine/patmos/include" # -> include
)
set( PACKAGE_ITEMS
	# Package binaries
	"bin/llc" "bin/llvm-link" "bin/clang-12" "bin/llvm-config" "bin/llvm-objdump" "bin/opt"
	
	# Moved binaries 
	"${PACKAGE_TEMP_DIR}/bin/clang" # Actually a symlink
	
	# Clang headers
	"lib/clang" 
	
	# Std headers
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include"
	
	${PACKAGE_ITEMS_LIBS}
)

add_custom_target(PatmosPackageTempDirs 
    "${CMAKE_COMMAND}" -E make_directory 
	"${PACKAGE_TEMP_DIR}/bin" 
	"${PACKAGE_TEMP_DIR}/lib" 
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib"
	"${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include"
)
add_custom_command(
	OUTPUT ${PACKAGE_ITEMS_LIBS}
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/libgloss/patmos/crt0.o" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/libgloss/patmos/crtbegin.o" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/libgloss/patmos/crtend.o" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"	
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/libgloss/patmos/libpatmos.a" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/newlib/libc/libc.a" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"
	COMMAND cp "../build-newlib/${PATMOS_TRIPLE}/newlib/libm/libm.a" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/"
	COMMAND cp "../build-compiler-rt/lib/generic/libclang_rt.builtins-patmos.a" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/librt.a"
	COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/patmos-libsyms.lst "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/lib/libsyms.lst"
	DEPENDS PatmosPackageTempDirs
)
add_custom_command(
	OUTPUT "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/newlib.h" # We need a target, but don't want to define all the files
	COMMAND rsync "../patmos-newlib/newlib/libc/include/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/"
	COMMAND rsync "../patmos-newlib/newlib/libc/include/machine/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/machine/"
	COMMAND rsync "../patmos-newlib/newlib/libc/include/rpc/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/rpc/"
	COMMAND rsync "../patmos-newlib/newlib/libc/include/sys/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/sys/"
	COMMAND rsync "../patmos-newlib/newlib/libc/machine/patmos/machine/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/machine/"
	COMMAND rsync "../patmos-newlib/newlib/libc/machine/patmos/sys/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/sys/"
	COMMAND rsync "../patmos-newlib/newlib/libc/machine/patmos/include/*" "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/"
)

ADD_CUSTOM_TARGET(symlink-clang
	COMMAND ${CMAKE_COMMAND} -E create_symlink patmos-clang-12 ${PACKAGE_TEMP_DIR}/bin/clang
	DEPENDS clang PatmosPackageTempDirs		  
	)		  
# Build release tarball containing binaries and metadata
add_custom_command(
	OUTPUT ${PACKAGE_TAR_GZ} 
		# We add all generated files to the list of outputs to ensure that cleaning will remove them.
		${PACKAGE_TAR} ${PACKAGE_INFO_FILE} 

	# Package binaries
	COMMAND tar -cf ${PACKAGE_TAR} 
		# Rename add "patmos-*" to all binary names
		--transform 's,bin/,bin/patmos-,' 
		# Remove temp dir from paths
		--transform 's,^${PACKAGE_TEMP_DIR}/,,'
		${PACKAGE_ITEMS}

	# Build YAML info file
	COMMAND ${CMAKE_COMMAND} -E echo "name: ${PROJECT_NAME}" > ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo "target: ${TARGET_TRIPLE}" >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo_append "version: " >> ${PACKAGE_INFO_FILE}
	COMMAND git describe --tags --always >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo_append "commit: " >> ${PACKAGE_INFO_FILE}
	COMMAND git rev-parse HEAD >> ${PACKAGE_INFO_FILE}
	COMMAND ${CMAKE_COMMAND} -E echo "files:\\| " >> ${PACKAGE_INFO_FILE}
	COMMAND tar -tf ${PACKAGE_TAR} | sed "s/^/  /" >> ${PACKAGE_INFO_FILE}
	
	# Add the info file to the package
	COMMAND tar -rf ${PACKAGE_TAR} ${PACKAGE_INFO_FILE}
		# Remove temp dir from paths
		--transform 's,^${PACKAGE_TEMP_DIR}/,,'
	
	# Compress the tar
	COMMAND gzip -9 < ${PACKAGE_TAR} > ${PACKAGE_TAR_GZ}
	
	DEPENDS ${PACKAGE_TARGETS} ${PACKAGE_ITEMS_LIBS} symlink-clang "${PACKAGE_TEMP_DIR}/${PATMOS_TRIPLE}/include/newlib.h"
)
# Rename release tarball target to something better.
add_custom_target(PatmosPackage DEPENDS ${PACKAGE_TAR_GZ})