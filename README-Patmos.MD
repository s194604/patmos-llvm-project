Patmos LLVM
================

This is a fork of the LLVM Project that adds a target for the Patmos Architecture.

# Development

### Requirements

Platform:

- Ubuntu 18/20
- MacOs
- Windows Subsystem for Linux running Ubuntu 18/20

Tools:

- `gcc` v9.1 or `clang` v11.0.0
- `git` v2.17.1
- `cmake` v4.0.0
- Patmos Simulator Tools v1.0.2

### Branches

- `master`: Primary development branch.
- `upstream`: Only for tracking the upstream LLVM repository. Should only be used to pull in new versions of LLVM and not to make any changes to them. [See more here](#anch-updating-llvm).

### Cloning

The LLVM project is very large so we can save some time by avoiding cloning the part we don't use for Patmos.
The following commands with clone only the parts of the Patmos LLVM repository that we actually use:

```
git clone --filter=blob:none --sparse git@github.com:t-crest/patmos-llvm-project
cd patmos-llvm-project
git sparse-checkout add /.github /llvm /clang
```

### Build

Ensure all the above reqruirements are met and that the tools are available on the PATH.

Then you must create a build folder.
This folder doesn't have to be in the LLVM Project root folder, but that is where we will put it in this example.
In this folder, you must setup the build with `cmake` (referencing the LLVM subfolder), after which you can build:

```
mkdir -p build
cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DLLVM_TARGETS_TO_BUILD=Patmos -DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf -DLLVM_ENABLE_PROJECTS="clang" -DCLANG_ENABLE_ARCMT=false -DCLANG_ENABLE_STATIC_ANALYZER=false -DCLANG_BUILD_EXAMPLES=false -DLLVM_ENABLE_BINDINGS=false -DLLVM_INSTALL_BINUTILS_SYMLINKS=false -DLLVM_INSTALL_CCTOOLS_SYMLINKS=false -DLLVM_INCLUDE_EXAMPLES=false -DLLVM_INCLUDE_BENCHMARKS=false -DLLVM_APPEND_VC_REV=false -DLLVM_ENABLE_WARNINGS=false -DLLVM_ENABLE_PEDANTIC=false -DLLVM_ENABLE_LIBPFM=false -DLLVM_BUILD_INSTRUMENTED_COVERAGE=false -DLLVM_INSTALL_UTILS=false
make -j
```
Where the $INSTALL_DIR should be the local installation folder. typically '../local'.

### Test

To run the tests you must first build following the previous section.
Then it is simple enough. From the build folder a single command can be used:

```
make check-all
```

### <a name="anch-updating-llvm"></a>Updating LLVM

Even though this repository is a fork of [the official LLVM repository](https://github.com/llvm/llvm-project), 
we only pull the upstream changes at an official release.
The `upstream` branch is specifically intended for this and nothing else.
Its `HEAD` should always be a commit for an official release.
E.g. it could be tracking the commit for the 11.0.0 release (176249b).

To update the version of LLVM used, the following steps must be taken:

* Switch to the `upstream` branch: `git checkout upstream`.
* Pull the latest release commit. This is most easily done by pull the corresponding tag (e.g. `llvmorg-11.0.0` for the 11.0.0 release):
`git pull https://github.com/llvm/llvm-project.git llvmorg-11.0.0`
* Switch to the `master` branch: `git checkout master`.
* Merge the `upstream` branch into `master`: `git merge upstream`
* Fix any merge conflicts that arise.
