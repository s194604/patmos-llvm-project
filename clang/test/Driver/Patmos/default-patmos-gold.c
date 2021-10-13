// RUN: %clang --target=patmos %s -o %t -Xlinker --bogus-patmos-gold-flag 2>&1 | \
// RUN: FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that patmos gold is searched for on the path by default.
// We test this by providing an illegal flag to it, and ensuring it fails with a message identifying
// as 'patmos-ld'
//
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() { }

// CHECK: patmos-ld: --bogus-patmos-gold-flag: unknown option
// CHECK: error: link  via llvm-link, opt and llc command failed