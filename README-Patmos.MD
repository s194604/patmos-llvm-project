Patmos LLVM
================

This is a fork of the LLVM Project that adds a target for the Patmos Architecture.

# Installation

Prebuilt releases are [published on Github](https://github.com/t-crest/patmos-llvm-project/releases) as `.tar.gz` files.
To install, download the tarball that fits your platform into you desired installation folder (usually `t-crest/local`.
Then open a terminal in that folder and run the following command (where `<tarfile>` is the release tar file you downloaded):

```
tar -xvf <tarfile>
```

This will correctly install all artifacts into the right paths. You are now ready to use the compiler.
Remember to set your `PATH` to point to `t-crest/local/bin`.

Also remember that you need [the Patmos Gold Linker](https://github.com/t-crest/patmos-gold) to be installed alongside the compiler.

# Development

### Requirements

Platform:

- Ubuntu 18/20
- MacOs 10.15
- Windows Subsystem for Linux running Ubuntu 18/20

Tools:

- `gcc` v9.1 or `clang` v11.0.0
- `git` v2.17.1
- `cmake` v3.21.0
- `make` v4.1
- GNU tar v1.29 (must be available on the path as `tar`)
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
git sparse-checkout add /.github /llvm /clang /compiler-rt
```

### Build

Ensure all the above reqruirements are met and that the tools are available on the PATH.

Then you must create a build folder.
This folder doesn't have to be in the LLVM Project root folder, but that is where we will put it in this example.
In this folder, you must setup the build with `cmake` (referencing the LLVM subfolder), after which you can build:

```
mkdir -p build
cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_TARGETS_TO_BUILD="Patmos" -DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf -DLLVM_ENABLE_PROJECTS="clang" -DCLANG_ENABLE_ARCMT=false -DCLANG_ENABLE_STATIC_ANALYZER=false -DCLANG_BUILD_EXAMPLES=false -DLLVM_ENABLE_BINDINGS=false -DLLVM_INSTALL_BINUTILS_SYMLINKS=false -DLLVM_INSTALL_CCTOOLS_SYMLINKS=false -DLLVM_INCLUDE_EXAMPLES=false -DLLVM_INCLUDE_BENCHMARKS=false -DLLVM_APPEND_VC_REV=false -DLLVM_ENABLE_WARNINGS=false -DLLVM_ENABLE_PEDANTIC=false -DLLVM_ENABLE_LIBPFM=false -DLLVM_BUILD_INSTRUMENTED_COVERAGE=false -DLLVM_INSTALL_UTILS=false
make -j
```

If you need to work on Compiler-RT, you can do the following (from the root folder):

```
mkdir -p build-compiler-rt
cd build-compiler-rt
cmake ../compiler-rt/ -DCMAKE_TOOLCHAIN_FILE=../compiler-rt/cmake/patmos-clang-toolchain.cmake -DCMAKE_PROGRAM_PATH="$(pwd)/../build/bin" -DCOMPILER_RT_INCLUDE_TESTS=ON
make -j
```

If you need to build the standard library (newlib):

```
git clone https://github.com/t-crest/patmos-newlib
mkdir build-newlib
cd build-newlib
../patmos-newlib/configure --target=patmos-unknown-unknown-elf AR_FOR_TARGET=ar CC_FOR_TARGET="$(pwd)/../build/bin/clang" CFLAGS_FOR_TARGET="-target patmos-unknown-unknown-elf -O2 -emit-llvm -D__GLIBC_USE\(...\)=0" RANLIB_FOR_TARGET=ranlib LD_FOR_TARGET="$(pwd)/../build/bin/clang"
make
```
This compiles std into bitcode files, such that they are ready to publish with the compiler.

### Test

To run the tests you must first build following the previous section.
Then it is simple enough. From the build folder we have 2 commands.

To test LLVM:

```
make -j llc && ./bin/llvm-lit ../llvm/test -v --filter=Patmos
```

To test Clang:

```
make -j ClangPatmosTestDeps && ./bin/llvm-lit ../clang/test -v --filter=Patmos
```

To test Compiler-RT, go to the `build-compiler-rt` folder:

```
./bin/llvm-lit -v test/builtins/Unit/patmos
```

### Packaging

To create a tarball containing the built compiler and standard library, use the following command from the `build` folder:

```
make PatmosPackage
```

To do this, you must have built LLVM, Compiler-RT, and `newlib` using the previous steps.
The tarball is then available under `build/patmos-unknown-unknown-elf/package-temp/patmos-llvm-*.tar.gz`, where `*` depends on the platform you are building on.
Using this tarball, you can install the compiler as described in the installation section.

_Note: The steps for building LLVM are meant for development and as such produce a debug-mode compiler (which is extremely slow). 
To package a release compiler, use the flag `-DCMAKE_BUILD_TYPE=Release`_

### Releasing New Versions

To fully automatically publish a new version, simply tag the required commit using `x.y.z` version numbering (with an optional `-` followed by anything).
Github Actions will then automatically test the commit, make packages for each platform, and publish them all.

### <a name="anch-updating-llvm"></a>Updating LLVM

Even though this repository is a fork of [the official LLVM repository](https://github.com/llvm/llvm-project), 
we only pull the upstream changes at official release tags.
The `upstream` branch is specifically intended for this and nothing else.
Its `HEAD` should always be a commit for an official release.
E.g. it could be tracking the commit for the 11.1.0 release.

Updating to a new release of upstream LLVM is not a trivial task.
The main difficulty comes from the fact that LLVM uses release branches, whic means their history splits after a major release. 
This means trying to simply pull in the latest release tag will likely cause many merge conflicts in LLVM code that we (the Patmos project) haven't touched.
Trying to fix the merge conflict can be tedious and error prone.
Instead, the following guide will mimick mergin by simply applying Patmos' code directly to the new release.
This will cause much fewer and simply conflicts and is much less likely to be done incorrectly:

_`llvm-upstream` refers to the LLVM github repo: github.com/llvm/llvm-project._

_`t-crest` refers to the Patmos LLVM github repo: github.com/t-crest/patmos-llvm-project._

_As an example, `llvmorg-11.1.0` is the current LLVM version tag used by Patmos, and `llvmorg-12.0.1` is the one we are trying to update to. Change them as appropriate_

* Get the most recent LLVM release's tag: 
```sh
git checkout master
git pull llvm-upstream --tags
```

* Update the `upstream` branch to point to it: 
```sh
git branch -f upstream llvmorg-12.0.1
```

* Push the new tag and "upstream" to the t-crest repo:
```sh
git checkout upstream
git push t-crest +upstream
git push llvmorg-12.0.1
```

* Create a diff of the difference between Patmos HEAD and `llvmorg-11.1.0`, which will then contain all Patmos-specific code/changes:
```sh
git diff llvmorg-11.1.0 master > patmos-diff.patch
```

* Move to a temporary branch where you apply the changes to the latest LLVM release:
```sh
git checkout llvmorg-12.0.1 -b tempbranch
git apply --reject --verbose --ignore-whitespace patmos-diff.patch
```

* Git has now applied all changes it can. Where unable, it left a `*.rej` file containing the unapplied patmos changes (where `*` is th name of the file containing the unapplied code).
Go through the project and apply rejected code manually and deleting the `*.rej` files as you go.
To check whether you applied the diff correctly, you can print out the changes (`git diff > post-apply-patmos-diff.patch`) and compare it with the original diff (`diff post-apply-patmos-diff.patch patmos-diff.patch`). 
The differences should be minor and only in non-patmos code.

* Commit: `git commit -m "Merged Patmos HEAD with llvmorg-12.0.1"`

* Rewrite git history, to show that the last Patmos commit is also a parent of the new one (to make it look like we merged it with `llvmorg-12.0.1`):
```sh
git replace --graft tempbranch llvmorg-12.0.1 master
git filter-branch llvmorg-12.0.1..tempbranch
```

* Change `master` to point to the new commit, delete the temporary branch, and push to T-CREST reposity:
```sh
git branch -f master tempbranch
git checkout master
git branch -D tempbranch
git push origin +master
```

You have now effectively merged the old Patmos head with the new LLVM release tag.
The git history will showing as being a merge of the two commits.
After this, the Patmos code will probably need tweaking to conform to any changes LLVM has made since the last release.
