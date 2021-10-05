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
  mutable std::unique_ptr<Tool> PatmosClang;
  Tool *getPatmosClang() const;
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
  RuntimeLibType GetDefaultRuntimeLibType() const override;
  void AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  Tool *buildStaticLibTool() const override {return buildLinker();};
protected:
  Tool *getTool(Action::ActionClass AC) const override;
  Tool *buildLinker() const override;

};

} // end namespace toolchains

namespace tools {
namespace patmos {
class PatmosBaseTool {
  const ToolChain &TC;
  std::vector<std::string> LibraryPaths;
public:
  PatmosBaseTool(const ToolChain &TC): TC(TC)
  {}

protected:
  // Some helper methods to construct arguments in ConstructJob
  llvm::file_magic getFileType(StringRef filename) const;

  llvm::file_magic getBufFileType(const char *buf) const;

  bool isBitcodeFile(StringRef filename) const {
    return getFileType(filename) == llvm::file_magic::bitcode;
  }

  bool isArchive(StringRef filename) const {
    return getFileType(filename) == llvm::file_magic::archive;
  }

  bool isDynamicLibrary(StringRef filename) const;

  bool isBitcodeArchive(StringRef filename) const;

  bool isBitcodeOption(StringRef Option,
                       const std::vector<std::string> &LibPaths) const;

  const char * CreateOutputFilename(Compilation &C, const InputInfo &Output,
                                    const char * TmpPrefix,
                                    const char *Suffix,
                                    bool IsLastPass) const;

  /// Get the option value of an argument
  std::string getArgOption(StringRef Arg) const;

  std::string FindLib(StringRef LibName,
                      const std::vector<std::string> &Directories,
                      bool OnlyStatic) const;

  std::vector<std::string> FindBitcodeLibPaths(const llvm::opt::ArgList &Args,
                                               bool LookupSysPaths) const;

  /// Get the last -O<Lvl> optimization level specifier. If no -O option is
  /// given, return NULL.
  llvm::opt::Arg* GetOptLevel(const llvm::opt::ArgList &Args, char &Lvl) const;

  /// Add -L arguments
  void AddLibraryPaths(const llvm::opt::ArgList &Args,
                       llvm::opt::ArgStringList &CmdArgs) const;

  /// The HasGoldPass arguments tells the function if
  /// we will execute gold or if linking with ELFs should throw an error.
  /// Return the
  const char * AddInputFiles(const llvm::opt::ArgList &Args,
                     const std::vector<std::string> &LibPaths,
                     const InputInfoList &Inputs,
                     llvm::opt::ArgStringList &LinkInputs,
                     llvm::opt::ArgStringList &GoldInputs,
                     const char *linkedBCFileName,
                     unsigned &linkedOFileInputPos,
                     bool AddLibSyms, bool LinkLibraries,
                     bool HasGoldPass, bool UseLTO) const;

  /// Return true if any options have been added to LinkInputs.
  bool AddSystemLibrary(const llvm::opt::ArgList &Args,
                        const std::vector<std::string> &LibPaths,
                        llvm::opt::ArgStringList &LinkInputs,
                        llvm::opt::ArgStringList &GoldInputs,
                        const char *libo, const char *libflag,
                        bool AddLibSyms, bool HasGoldPass, bool UseLTO) const;

  /// Add arguments to link with libc, librt, librtsf, libpatmos
  void AddStandardLibs(const llvm::opt::ArgList &Args,
                       const std::vector<std::string> &LibPaths,
                       llvm::opt::ArgStringList &LinkInputs,
                       llvm::opt::ArgStringList &GoldInputs,
                       bool AddRuntimeLibs, bool AddLibGloss, bool AddLibC,
                       bool AddLibSyms, StringRef FloatABI,
                       bool HasGoldPass, bool UseLTO) const;


  /// Returns linkedBCFileName if files need to be linked, or the filename of
  /// the only bitcode input file if there is no need to link, or null if
  /// there are no bitcode inputs.
  /// @linkedOFileInsertPos - position in GoldInputs where to insert the
  /// compiled bitcode file into.
  const char * PrepareLinkerInputs(const llvm::opt::ArgList &Args,
                       const InputInfoList &Inputs,
                       llvm::opt::ArgStringList &LinkInputs,
                       llvm::opt::ArgStringList &GoldInputs,
                       const char *linkedBCFileName,
                       unsigned &linkedOFileInsertPos,
                       bool AddStartFiles,
                       bool AddRuntimeLibs, bool AddLibGloss, bool AddLibC,
                       bool AddLibSyms, StringRef FloatABI,
                       bool LinkLibraries,
                       bool HasGoldPass, bool UseLTO) const;

  void ConstructLinkJob(const Tool &Creator, Compilation &C,
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
                       const llvm::opt::ArgList &TCArgs,
                       bool IsLinkPass, bool IsLastPass) const;

  void ConstructLLCJob(const Tool &Creator, Compilation &C,
                    const JobAction &JA,
                    const InputInfo &Output,
                    const InputInfoList &Inputs,
                    const char *OutputFilename,
                    const char *InputFilename,
                    const llvm::opt::ArgList &TCArgs,
                    bool EmitAsm) const;

  void ConstructGoldJob(const Tool &Creator, Compilation &C,
                        const JobAction &JA,
                        const InputInfo &Output,
                        const InputInfoList &Inputs,
                        const char *OutputFilename,
                        const llvm::opt::ArgStringList &GoldInputs,
                        const llvm::opt::ArgList &TCArgs,
                        bool LinkRelocatable, bool AddStackSymbols) const;
};

class LLVM_LIBRARY_VISIBILITY Compile : public Clang, protected PatmosBaseTool
{
public:
  Compile(const ToolChain &TC) : Clang(TC), PatmosBaseTool(TC) {}

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output,
                    const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;

  bool hasIntegratedAssembler() const override { return false; }
};

class LLVM_LIBRARY_VISIBILITY Linker : public Tool, protected PatmosBaseTool {
public:
  Linker(const ToolChain &TC) : Tool("patmos::Link",
                                     "link  via llvm-link, opt and llc", TC),
                                PatmosBaseTool(TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

void getPatmosTargetFeatures(const Driver &D, const llvm::Triple &Triple,
                            const llvm::opt::ArgList &Args,
                            std::vector<llvm::StringRef> &Features);
StringRef getPatmosABI(const llvm::opt::ArgList &Args,
                      const llvm::Triple &Triple);
StringRef getPatmosArch(const llvm::opt::ArgList &Args,
                       const llvm::Triple &Triple);

} // end namespace Patmos
} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_PATMOS_H
