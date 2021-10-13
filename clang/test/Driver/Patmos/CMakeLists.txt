add_custom_target(ClangPatmosTestDeps)
add_dependencies(clang-test-depends ClangPatmosTestDeps)

add_subdirectory(mock-binaries)
add_subdirectory(mock-includes)
add_subdirectory(mock-std-libs)
