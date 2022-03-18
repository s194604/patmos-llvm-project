//===--- Patmos.h - Patmos ToolChain Implementations -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PATMOS_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PATMOS_H

#include "Gnu.h"
#include "clang/Driver/ToolChain.h"
#include "Clang.h"
#include "llvm/BinaryFormat/Magic.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY PatmosToolChain : public ToolChain {
private:
  mutable std::unique_ptr<Tool> Compile;
  mutable std::unique_ptr<Tool> FinalLink;
public:
  PatmosToolChain(const Driver &D, const llvm::Triple &Triple,
                 const llvm::opt::ArgList &Args);

  bool IsIntegratedAssemblerDefault() const override { return true; }
  bool IsUnwindTablesDefault(const llvm::opt::ArgList &Args) const override {
    return false;
  }
  bool SupportsProfiling() const override { return false; }
  bool isPICDefault() const override { return false; }
  bool isPIEDefault() const override { return false; }
  bool isPICDefaultForced() const override { return false; }
  bool isCrossCompiling() const override { return true;};
  Tool *SelectTool(const JobAction &JA) const override;
  void AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                              llvm::opt::ArgStringList &CC1Args) const override;
  Tool *getPatmosCompile() const;
  Tool *getPatmosFinalLink() const;

};

} // end namespace toolchains

namespace tools {
namespace patmos {
class PatmosBaseTool {
public:
  PatmosBaseTool(const clang::driver::toolchains::PatmosToolChain &TC): TC(TC)
  {}

protected:
  const clang::driver::toolchains::PatmosToolChain &TC;

  const char * CreateOutputFilename(Compilation &C, const InputInfo &Output,
                                    const char * TmpPrefix,
                                    const char *Suffix,
                                    bool IsLastPass) const;

  std::string getLibPath(const char* LibName) const;
  /// Get the last -O<Lvl> optimization level specifier. If no -O option is
  /// given, return NULL.
  llvm::opt::Arg* GetOptLevel(const llvm::opt::ArgList &Args, char &Lvl) const;

  void PrepareLink1Inputs(const llvm::opt::ArgList &Args,
                       const InputInfoList &Inputs,
                       llvm::opt::ArgStringList &LinkInputs) const;
  void PrepareLink2Inputs(const llvm::opt::ArgList &Args,
                       const char* Input,
                       llvm::opt::ArgStringList &LinkInputs) const;
  void PrepareLink3Inputs(const llvm::opt::ArgList &Args,
                       const char* Input,
                       llvm::opt::ArgStringList &LinkInputs) const;
  void PrepareLink4Inputs(const llvm::opt::ArgList &Args,
                       const char* Input,
                       llvm::opt::ArgStringList &LinkInputs) const;

  void ConstructLLVMLinkJob(const Tool &Creator, Compilation &C,
                        const JobAction &JA,
                        const InputInfo &Output,
                        const InputInfoList &Inputs,
                        const char *OutputFilename,
                        const llvm::opt::ArgStringList &LinkInputs,
                        const llvm::opt::ArgList &TCArgs) const;

  // Construct an optimization job
  // @IsLinkPass - If true, add standard link optimizations
  bool ConstructOptJob(const Tool &Creator, Compilation &C,
                       const JobAction &JA,
                       const InputInfo &Output,
                       const InputInfoList &Inputs,
                       const char *OutputFilename,
                       const char *InputFilename,
                       const llvm::opt::ArgList &TCArgs) const;

  void ConstructLLCJob(const Tool &Creator, Compilation &C,
                    const JobAction &JA,
                    const InputInfo &Output,
                    const InputInfoList &Inputs,
                    const char *OutputFilename,
                    const char *InputFilename,
                    const llvm::opt::ArgList &TCArgs) const;

  void ConstructLLDJob(const Tool &Creator, Compilation &C,
                        const JobAction &JA,
                        const InputInfo &Output,
                        const InputInfoList &Inputs,
                        const char *OutputFilename,
                        const llvm::opt::ArgStringList &LLDInputs,
                        const llvm::opt::ArgList &TCArgs,
                        bool AddStackSymbols) const;
};

class LLVM_LIBRARY_VISIBILITY Compile : public Clang, protected PatmosBaseTool
{
public:
  Compile(const clang::driver::toolchains::PatmosToolChain &TC) : Clang(TC), PatmosBaseTool(TC) {}

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output,
                    const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
  bool hasIntegratedAssembler() const override { return true; }
};

class LLVM_LIBRARY_VISIBILITY FinalLink : public Tool, protected PatmosBaseTool {
public:
  FinalLink(const clang::driver::toolchains::PatmosToolChain &TC) : Tool("patmos::FinalLink",
                                     "Link final executable", TC),
                                PatmosBaseTool(TC)
  {}
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
  bool isLinkJob() const override { return true; }
  bool hasIntegratedCPP() const override { return false; }
};

} // end namespace Patmos
} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PATMOS_H
