//===--- Patmos.cpp - Patmos ToolChain Implementations ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Patmos.h"
#include "CommonArgs.h"
#include "Clang.h"
#include "InputInfo.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/ObjCRuntime.h"
#include "clang/Basic/Version.h"
#include "clang/Driver/Action.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Job.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/SanitizerArgs.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Driver/Util.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Config/config.h"
#include "llvm/Object/Archive.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// Patmos Toolchain
PatmosToolChain::PatmosToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  // Get install path to find tools and libraries
  std::string Path(D.getInstalledDir());

  // tools?
  getProgramPaths().push_back(Path);
  if (llvm::sys::fs::exists(Path + "/bin/"))
    getProgramPaths().push_back(Path + "/bin/");
  if (llvm::sys::fs::exists(Path + "/../bin/"))
    getProgramPaths().push_back(Path + "/../bin/");

  std::string TripleString = Triple.getTriple();

  // newlib libraries and includes
  if (llvm::sys::fs::exists(Path + "/" + TripleString + "/"))
    getFilePaths().push_back(Path + "/" + TripleString + "/");
  if (llvm::sys::fs::exists(Path + "/../" + TripleString + "/"))
    getFilePaths().push_back(Path + "/../" + TripleString + "/");
}

Tool *PatmosToolChain::getTool(Action::ActionClass AC) const
{
  if (AC == Action::CompileJobClass) {
    // Use a special clang driver that supports compiling to ELF with
    // -fpatmos-emit-obj
    return getPatmosClang();
  }
  return ToolChain::getTool(AC);
}

Tool *PatmosToolChain::getPatmosClang() const {
  if (!PatmosClang)
    PatmosClang.reset(new tools::patmos::Compile(*this));
  return PatmosClang.get();
}

Tool *PatmosToolChain::buildLinker() const {
  return new tools::patmos::Linker(*this);
}

ToolChain::RuntimeLibType PatmosToolChain::GetDefaultRuntimeLibType() const {
  return ToolChain::ToolChain::RLT_CompilerRT;
}

void PatmosToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  if (!DriverArgs.hasArg(options::OPT_nostdinc) &&
      !DriverArgs.hasArg(options::OPT_nostdlibinc)) {
    const ToolChain::path_list &filePaths = getFilePaths();
    for(ToolChain::path_list::const_iterator i = filePaths.begin(),
        ie = filePaths.end(); i != ie; i++) {
      // construct a library search path
      CC1Args.push_back("-isystem");
      CC1Args.push_back(DriverArgs.MakeArgString(Twine(*i) + "include/"));
    }
  }
}

llvm::file_magic
patmos::PatmosBaseTool::getFileType(StringRef filename) const {
  llvm::file_magic magic;
  if (llvm::identify_magic(filename , magic)) {
    return llvm::file_magic::unknown;
  }

  return magic;
}

llvm::file_magic
patmos::PatmosBaseTool::getBufFileType(const char *buf) const {
  std::string magic(buf, 4);
  return llvm::identify_magic(magic);
}

bool patmos::PatmosBaseTool::isDynamicLibrary(StringRef filename) const {
  llvm::file_magic type = getFileType(filename);
  switch (type) {
    default: return false;
    case llvm::file_magic::macho_fixed_virtual_memory_shared_lib:
    case llvm::file_magic::macho_dynamically_linked_shared_lib:
    case llvm::file_magic::macho_dynamically_linked_shared_lib_stub:
    case llvm::file_magic::elf_shared_object:
    case llvm::file_magic::pecoff_executable:  return true;
  }
}

bool patmos::PatmosBaseTool::isBitcodeArchive(StringRef filename) const {

  if (getFileType(filename) != llvm::file_magic::archive) {
    return false;
  }

  // check first file in archive if it is a bitcode file

  std::unique_ptr<llvm::object::Binary> File;
  if (llvm::object::createBinary(filename)) {
    return false;
  }

  if (llvm::object::Archive *a = dyn_cast<llvm::object::Archive>(File.get())) {
    auto err = llvm::Error::success();
    for (auto iter = a->child_begin(err), end = a->child_end(); !err && iter != end; ++iter) {
      if (iter->getAsBinary()) {
        // Try opening it as a bitcode file.
        if (auto buff = iter->getMemoryBufferRef()) {
          llvm::file_magic FileType =
                   getBufFileType(buff->getBufferStart());
          if (FileType == llvm::file_magic::bitcode) {
            return true;
          }
          if (FileType != llvm::file_magic::unknown) {
            return false;
          }
        }
      }
    }
  }

  return false;
}

const char * patmos::PatmosBaseTool::CreateOutputFilename(Compilation &C,
    const InputInfo &Output, const char * TmpPrefix, const char *Suffix,
    bool IsLastPass) const
{
  const char * filename = NULL;

  const ArgList &Args = C.getArgs();
  const Driver &D = TC.getDriver();

  if (IsLastPass) {
    if (Output.isFilename()) {
      filename = Args.MakeArgString(Output.getFilename());
    }
    else {
      // write to standard-out if nothing is given?!?
      filename = "-";
    }
  } else {
    if (Args.hasArg(options::OPT_save_temps) && Output.isFilename()) {
      // take the output's name and append a suffix
      std::string name(Output.getFilename());
      filename = Args.MakeArgString((name + "." + Suffix).c_str());
    }
    else {
      StringRef Name = Output.isFilename() ?
                    llvm::sys::path::filename(Output.getFilename()) : TmpPrefix;
      std::pair<StringRef, StringRef> Split = Name.split('.');
      std::string TmpName = D.GetTemporaryPath(Split.first, Suffix);
      filename = Args.MakeArgString(TmpName.c_str());
      C.addTempFile(filename);
    }
  }
  return filename;
}

std::string patmos::PatmosBaseTool::getArgOption(StringRef Option) const
{
  // TODO Check for --<name>=value options??

  if (Option.size() > 2 && Option[2] == '=') {
    return std::string(Option.substr(3));
  }
  else if (Option.size() > 2) {
    return std::string(Option.substr(2));
  }
  return "";
}

// Reimplement Linker::FindLib() to search for shared libraries first
// unless OnlyStatic is true.
std::string patmos::PatmosBaseTool::FindLib(StringRef LibName,
                         const std::vector<std::string> &Directories,
                         bool OnlyStatic) const
{
  if (llvm::sys::fs::exists(LibName) &&
      (isArchive(LibName) || (!OnlyStatic && isDynamicLibrary(LibName))))
    return std::string(LibName);

  // Now iterate over the directories
  for (std::vector<std::string>::const_iterator Iter = Directories.begin();
       Iter != Directories.end(); ++Iter) {
    SmallString<128> FullPath(*Iter);

    llvm::sys::path::append(FullPath, ("lib" + LibName).str());

    // adding a dummy extension so that replace_extension does the right thing
    FullPath += ".dummy";

    // Either we only want static libraries or we didn't find a
    // dynamic library so try libX.a
    llvm::sys::path::replace_extension(FullPath, "a");
    if (isArchive(FullPath.str()))
      return std::string(FullPath);

    // libX.bca
    llvm::sys::path::replace_extension(FullPath, "bca");
    if (isArchive(FullPath.str()))
      return std::string(FullPath);

    if (!OnlyStatic) {
      // Try libX.so or libX.dylib
      // TODO is there a better way to get the shared-lib file extension?
      llvm::sys::path::replace_extension(FullPath, LTDL_SHLIB_EXT);
      if (isDynamicLibrary(FullPath.str()) || // Native shared library
          isBitcodeFile(FullPath.str()))      // .so containing bitcode
      {
        return std::string(FullPath);
      }
    }
  }

  // No libraries were found
  return "";
}

std::vector<std::string>
patmos::PatmosBaseTool::FindBitcodeLibPaths(const ArgList &Args,
                                            bool LookupSysPaths) const
{
  ArgStringList LibArgs;

  // Use this little trick to prevent duplicating the library path options code
  AddLibraryPaths(Args, LibArgs);

  // To make sure that we catch all -L options of llvm-link, we add -Wl and
  // -Xlinker.
  for (ArgList::const_iterator it = Args.begin(), ie = Args.end(); it != ie;
      ++it)
  {
    const Arg *A = *it;

    if (A->getOption().matches(options::OPT_Wl_COMMA) ||
        A->getOption().matches(options::OPT_Xlinker))
    {
      A->renderAsInput(Args, LibArgs);
    }
  }

  // Parse all -L options
  std::vector<std::string> LibPaths;
  for (ArgStringList::iterator it = LibArgs.begin(), ie = LibArgs.end();
       it != ie; ++it) {
    std::string Arg = *it;
    if (Arg.substr(0, 2) != "-L") continue;

    LibPaths.push_back(getArgOption(Arg));
  }

  // Collect all the lookup paths
  if (LookupSysPaths) {
    // Add same paths as Linker::addSystemPaths().
    // No use in checking gold linker system paths, if not found we link
    // using gold in any case.
    LibPaths.insert(LibPaths.begin(), std::string("./"));
  }

  return LibPaths;
}

bool patmos::PatmosBaseTool::isBitcodeOption(StringRef Option,
                     const std::vector<std::string> &LibPaths) const
{
  if (Option.str().substr(0,2) != "-l") {
    // standard input file
    return isBitcodeFile(Option);
  }

  std::string LibName = getArgOption(Option);

  std::string Filename = FindLib(LibName, LibPaths, false);
  if (!Filename.empty()) {
    // accept linking with bitcode files
    return isBitcodeFile(Filename) || isBitcodeArchive(Filename);
  }
  return false;
}


Arg* patmos::PatmosBaseTool::GetOptLevel(const ArgList &Args, char &Lvl) const {

  if (Arg *A = Args.getLastArg(options::OPT_O_Group)) {
    std::string Opt = A->getAsString(Args);
    if (Opt.length() != 3) {
      llvm::report_fatal_error("Unsupported optimization option: " + Opt);
    }
    Lvl = Opt[2];
    return A;
  }
  return 0;
}

void patmos::PatmosBaseTool::AddLibraryPaths(const ArgList &Args,
                                             ArgStringList &CmdArgs) const
{
  Args.AddAllArgs(CmdArgs, options::OPT_L);

  addDirectoryList(Args, CmdArgs, "-L", "LIBRARY_PATH");

  // append default search paths
  const ToolChain::path_list &filePaths = TC.getFilePaths();
  for(ToolChain::path_list::const_iterator i = filePaths.begin(),
      ie = filePaths.end(); i != ie; i++) {
    // construct a library search path
    std::string path("-L" + *i + "lib/");
    CmdArgs.push_back(Args.MakeArgString(path.c_str()));
  }
}

const char * patmos::PatmosBaseTool::AddInputFiles(const ArgList &Args,
                           const std::vector<std::string> &LibPaths,
                           const InputInfoList &Inputs,
                           ArgStringList &LinkInputs, ArgStringList &GoldInputs,
                           const char *linkedBCFileName,
                           unsigned &linkedOFileInputPos,
                           bool AddLibSyms, bool LinkDefaultLibs,
                           bool HasGoldPass, bool UseLTO) const
{
  const Driver &D = TC.getDriver();

  const char *BCOutput = 0;

  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;

    //--------------------------------------------------------------------------
    // handle file inputs
    if (II.isFilename()) {

      // Check for bitcode files
      bool IsBitcode = false;

      if (II.getType() == types::TY_AST) {
        D.Diag(diag::err_drv_no_ast_support) << TC.getTripleString();
      }
      else if (II.getType() == types::TY_LLVM_BC ||
                 II.getType() == types::TY_LTO_BC) {
        IsBitcode = true;
      }
      else if (II.getType() == types::TY_Object) {

        llvm::file_magic FT = getFileType(II.getFilename());

        if (FT == llvm::file_magic::archive) {
          // Should we skip .a files without -l if the do not link in libs?
          IsBitcode = isBitcodeArchive(II.getFilename());
        }
        else if (FT == llvm::file_magic::bitcode) {
          IsBitcode = true;
        }
        else {
          // Some sort of binary file.. link with gold, even if LTO is not used
          if (!HasGoldPass) {
            // TODO use D.Diag() ?
            llvm::report_fatal_error(Twine(II.getFilename()) +
                                ": Cannot link binary files when "
                                "generating bitcode or object file output");
          }
        }
      }
      else {
        // Unhandled input file type
        D.Diag(diag::err_drv_no_linker_llvm_support) << TC.getTripleString();
      }

      if (IsBitcode && !UseLTO) {
        if (BCOutput) {
          // We already have at least one bitcode file, use temp output file
          BCOutput = linkedBCFileName;
        } else {
          // First bitcode file, link compiled file with gold
          BCOutput = II.getFilename();
          linkedOFileInputPos = GoldInputs.size();
        }
        LinkInputs.push_back(II.getFilename());
      } else {
        GoldInputs.push_back(II.getFilename());
      }

    }
    //--------------------------------------------------------------------------
    // handle -l options
    else {
      const Arg &A = II.getInputArg();

      // Reverse translate some rewritten options.
      if (A.getOption().matches(options::OPT_Z_reserved_lib_stdcxx)) {
        if (LinkDefaultLibs) {
          bool IsBitcode = !UseLTO && isBitcodeOption("-lstdc++", LibPaths);
          if (IsBitcode && !BCOutput) {
            llvm::report_fatal_error(Twine(II.getAsString()) +
                                ": Cannot link bitcode library without a "
                                "previous bitcode input file.");
          }
          (IsBitcode ? LinkInputs : GoldInputs).push_back("-lstdc++");
        }
        continue;
      }
      else if (A.getOption().matches(options::OPT_Wl_COMMA) ||
               A.getOption().matches(options::OPT_Xlinker) ||
               A.getOption().matches(options::OPT_L)) {
        // already handled
        continue;
      }
      else if (A.getOption().matches(options::OPT_l)) {

        // -l is marked as LinkerInput, so we should always get all -l flags
        // here, in the correct order.
        A.claim();

        if (!LinkDefaultLibs) {
          continue;
        }

        // -lm is special .. we handle this like a runtime library (should we?)
        // since we need to link in the libsyms stuff.
        if (getArgOption(A.getAsString(Args)) == "m") {

          if (AddSystemLibrary(Args, LibPaths, LinkInputs, GoldInputs,
                              "lib/libmsyms.o", "-lm",
                              AddLibSyms, HasGoldPass, UseLTO))
          {
            if (!BCOutput) {
              linkedOFileInputPos = GoldInputs.size();
            }
            BCOutput = linkedBCFileName;
          }
          continue;
        }

        bool IsBitcode = !UseLTO &&
                         isBitcodeOption(A.getAsString(Args), LibPaths);

        // Don't render as input
        A.render(Args, (IsBitcode ? LinkInputs : GoldInputs));

        if (IsBitcode) {
          if (BCOutput) {
            // We already have at least one bitcode file, use temp output file
            BCOutput = linkedBCFileName;
          } else {
            // First bitcode file is -l, this will not work
            llvm::report_fatal_error(Twine(A.getAsString(Args)) +
                                ": Cannot link bitcode library without a "
                                "previous bitcode input file.");
          }
        }
      } else {
        // Uh, what kind of option can this be, and what should we do with it?
        llvm::report_fatal_error(Twine(A.getAsString(Args)) +
                            ": unknown linker option.");

      }
    }
  }

  return BCOutput;
}

bool patmos::PatmosBaseTool::AddSystemLibrary(const ArgList &Args,
                        const std::vector<std::string> &LibPaths,
                        ArgStringList &LinkInputs, ArgStringList &GoldInputs,
                        const char *libo, const char *libflag,
                        bool AddLibSyms, bool HasGoldPass, bool UseLTO) const
{
  bool IsBitcode = isBitcodeOption(libflag, LibPaths);

  if (IsBitcode && AddLibSyms && libo) {
    std::string ofile = TC.GetFilePath(libo);
    (UseLTO ? GoldInputs : LinkInputs).push_back(Args.MakeArgString(ofile));
  }

  if (libflag) {
    (UseLTO || !IsBitcode ? GoldInputs : LinkInputs).push_back(libflag);
  }

  return !UseLTO && IsBitcode && ((AddLibSyms && libo) || libflag);
}


void patmos::PatmosBaseTool::AddStandardLibs(const ArgList &Args,
                           const std::vector<std::string> &LibPaths,
                           ArgStringList &LinkInputs, ArgStringList &GoldInputs,
                           bool AddRuntimeLibs, bool AddLibGloss, bool AddLibC,
                           bool AddLibSyms, StringRef FloatABI,
                           bool HasGoldPass, bool UseLTO) const
{
  // link by default with newlib libc and libpatmos
  if (AddLibC) {
    AddSystemLibrary(Args, LibPaths, LinkInputs, GoldInputs,
                     "lib/libcsyms.o", "-lc",
                     AddLibSyms, HasGoldPass, UseLTO);
  }

  // Add support library for newlib libc
  if (AddLibGloss) {
    AddSystemLibrary(Args, LibPaths, LinkInputs, GoldInputs,
                     0, "-lpatmos",
                     AddLibSyms, HasGoldPass, UseLTO);
  }

  // link by default with compiler-rt
  if (AddRuntimeLibs) {

    // softfloat has dependencies to librt, link first
    if (FloatABI != "hard" && FloatABI != "none") {
      AddSystemLibrary(Args, LibPaths, LinkInputs, GoldInputs,
                       "lib/librtsfsyms.o", "-lrtsf",
                       AddLibSyms, HasGoldPass, UseLTO);
    }

    AddSystemLibrary(Args, LibPaths, LinkInputs, GoldInputs,
                     "lib/librtsyms.o", "-lrt",
                     AddLibSyms, HasGoldPass, UseLTO);
  }

}

const char * patmos::PatmosBaseTool::PrepareLinkerInputs(const ArgList &Args,
                     const InputInfoList &Inputs,
                     ArgStringList &LinkInputs, ArgStringList &GoldInputs,
                     const char *linkedBCFileName,
                     unsigned &linkedOFileInsertPos,
                     bool AddStartFiles,
                     bool AddRuntimeLibs, bool AddLibGloss, bool AddLibC,
                     bool AddLibSyms, StringRef FloatABI, bool LinkDefaultLibs,
                     bool HasGoldPass, bool UseLTO) const
{
  const char* BCOutput = 0;
  linkedOFileInsertPos = 0;

  // prepare library lookups
  // do not add system paths as we call the linker with -nostdlib.
  std::vector<std::string> BCLibPaths = FindBitcodeLibPaths(Args, false);

  //----------------------------------------------------------------------------
  // append library search paths (-L) to bitcode linker

  if (LinkDefaultLibs) {
    AddLibraryPaths(Args, LinkInputs);
  }


  //----------------------------------------------------------------------------
  // link with start-up files crt0.o and crtbegin.o

  bool AddCrtBeginEnd = AddStartFiles &&
                        (TC.getTriple().getOS() != llvm::Triple::RTEMS);

  if (AddStartFiles) {
    std::string Crt0Filename = TC.GetFilePath("lib/crt0.o");
    std::string CrtBeginFilename = TC.GetFilePath("lib/crtbegin.o");

    if (isBitcodeFile(Crt0Filename)) {
      const char * crt0 = Args.MakeArgString(Crt0Filename);
      LinkInputs.push_back(crt0);

      if (AddCrtBeginEnd) {
        const char * crtbegin = Args.MakeArgString(CrtBeginFilename);
        LinkInputs.push_back(crtbegin);
      }

      BCOutput = BCOutput ? linkedBCFileName : crt0;
    } else {
      GoldInputs.push_back(Args.MakeArgString(Crt0Filename));
      linkedOFileInsertPos++;

      if (AddCrtBeginEnd) {
        GoldInputs.push_back(Args.MakeArgString(CrtBeginFilename));
        linkedOFileInsertPos++;
      }
    }
  }


  //----------------------------------------------------------------------------
  // append input files

  const char * InputFile = AddInputFiles(Args, BCLibPaths,
                Inputs, LinkInputs, GoldInputs,
                linkedBCFileName, linkedOFileInsertPos,
                AddLibSyms, LinkDefaultLibs, HasGoldPass, UseLTO);

  if (InputFile) {
    BCOutput = (BCOutput ? linkedBCFileName : InputFile);
  }


  //----------------------------------------------------------------------------
  // link with newlib and compiler-rt libraries

  if (LinkDefaultLibs) {
    size_t OldSize = LinkInputs.size();

    AddStandardLibs(Args, BCLibPaths, LinkInputs, GoldInputs,
                    AddRuntimeLibs, AddLibGloss, AddLibC,
                    AddLibSyms, FloatABI, HasGoldPass, UseLTO);

    // if we added some libs, run link pass
    if (LinkInputs.size() > OldSize && (BCOutput || AddLibSyms)) {
     BCOutput = linkedBCFileName;
    }
  }

  //----------------------------------------------------------------------------
  // link with start-up file crtend.o

  if (AddCrtBeginEnd) {
    std::string CrtEndFilename = TC.GetFilePath("lib/crtend.o");

    if (isBitcodeFile(CrtEndFilename)) {
      const char * crtend = Args.MakeArgString(CrtEndFilename);
      LinkInputs.push_back(crtend);
    } else {
      GoldInputs.push_back(Args.MakeArgString(CrtEndFilename));
      linkedOFileInsertPos++;
    }
  }


  //----------------------------------------------------------------------------
  // append -L options to gold, but only if we actually need it

  if (LinkDefaultLibs && !GoldInputs.empty()) {
    ArgStringList TmpArgs;

    AddLibraryPaths(Args, TmpArgs);

    GoldInputs.insert(GoldInputs.begin(), TmpArgs.begin(), TmpArgs.end());
    linkedOFileInsertPos += TmpArgs.size();
  }


  return BCOutput;
}

static std::string get_patmos_tool(const ToolChain &TC, StringRef ToolName)
{
  std::string longname = (TC.getTriple().str() + "-" + ToolName).str();
  std::string tmp(TC.GetProgramPath(longname.c_str()) );
  if (tmp != longname)
    return tmp;

  std::string PatmosTool = ("patmos-" + ToolName).str();
  tmp = TC.GetProgramPath(PatmosTool.c_str());
  if (tmp != PatmosTool)
    return tmp;

  return TC.GetProgramPath(ToolName.str().c_str());
}

void patmos::PatmosBaseTool::ConstructLinkJob(const Tool &Creator,
                     Compilation &C, const JobAction &JA,
                     const InputInfo &Output,
                     const InputInfoList &Inputs,
                     const char *OutputFilename,
                     const ArgStringList &LinkInputs,
                     const ArgList &Args) const
{
  ArgStringList CmdArgs;

  //----------------------------------------------------------------------------
  // append linker specific options

  // This must match the argument for FindBitcodeLibPaths in PrepareLinkerInputs
  CmdArgs.push_back("-nostdlib");

  for (ArgList::const_iterator
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_Wl_COMMA) ||
        A->getOption().matches(options::OPT_Xlinker)) {
      A->claim();
      A->renderAsInput(Args, CmdArgs);
    }
    else if ((A->getOption().hasFlag(options::LinkerInput) &&
              !A->getOption().matches(options::OPT_l)) ||
             A->getOption().matches(options::OPT_v))
    {
      // It is unfortunate that we have to claim here, as this means
      // we will basically never report anything interesting for
      // platforms using a generic gcc, even if we are just using gcc
      // to get to the assembler.
      A->claim();
      A->render(Args, CmdArgs);
    }
  }

  //----------------------------------------------------------------------------
  // append output file for linking

  assert(OutputFilename);
  CmdArgs.push_back("-o");
  CmdArgs.push_back(OutputFilename);

  //----------------------------------------------------------------------------
  // append input arguments

  CmdArgs.append(LinkInputs.begin(), LinkInputs.end());

  //----------------------------------------------------------------------------
  // execute the linker command

  const char *Exec = Args.MakeArgString(get_patmos_tool(TC, "llvm-link"));
  C.addCommand(std::make_unique<Command>(
      JA, Creator, ResponseFileSupport::AtFileCurCP(),
      Exec, CmdArgs, Inputs, Output));
}


bool patmos::PatmosBaseTool::ConstructOptJob(const Tool &Creator,
                     Compilation &C, const JobAction &JA,
                     const InputInfo &Output,
                     const InputInfoList &Inputs,
                     const char *OutputFilename,
                     const char *InputFilename,
                     const ArgList &Args,
                     bool IsLinkPass, bool IsLastPass) const
{
  ArgStringList OptArgs;

  int OptLevel = 0;
  char Lvl;
  Arg *OptArg;
  if ((OptArg = GetOptLevel(Args, Lvl))) {
    switch (Lvl) {
    case '0': OptLevel = 0; break;
    case '1': OptLevel = 1; break;
    case '2': OptLevel = 2; break;
    case '3': OptLevel = 3; break;
    // these two need to be > 0, otherwise no opt is triggered
    case 's': case 'z': OptLevel = 7; break;
    }
  }

  //----------------------------------------------------------------------------
  // append optimization options

  if (IsLinkPass) {
    if (OptLevel > 0) {

      // pass -O level to opt verbatim
      OptArg->renderAsInput(Args, OptArgs);

      // for some reason, we need to add this manually
      OptArgs.push_back("-internalize");
      OptArgs.push_back("-globaldce");
    }

    if (OptLevel == 3) {
      OptArgs.push_back("-std-link-opts");
    }

  }

  //----------------------------------------------------------------------------
  // append output and input files

  if (!IsLastPass && OptArgs.empty()) {
    return false;
  }

  assert(OutputFilename);
  OptArgs.push_back("-o");
  OptArgs.push_back(OutputFilename);

  OptArgs.push_back(InputFilename);

  //----------------------------------------------------------------------------
  // execute opt command

  const char *OptExec = Args.MakeArgString(get_patmos_tool(TC, "opt"));
  C.addCommand(std::make_unique<Command>(
      JA, Creator, ResponseFileSupport::AtFileCurCP(),
      OptExec, OptArgs, Inputs, Output));

  return true;
}

static StringRef getPatmosFloatABI(const Driver &D, const ArgList &Args,
                                   const llvm::Triple &Triple,
                                   bool &DefaultChanged)
{
  // Select the float ABI as determined by -msoft-float, -mhard-float,
  // and -mfloat-abi=.

  // TODO determine default FloatABI based on the processor subtarget features
  StringRef FloatABI = "soft";
  // Set to true when the user selects something different from the (subtarget) default.
  DefaultChanged = false;

  if (Arg *A = Args.getLastArg(options::OPT_msoft_float,
                               options::OPT_mhard_float,
                               options::OPT_mfloat_abi_EQ,
                               options::OPT_mno_soft_float)) {
    if (A->getOption().matches(options::OPT_msoft_float)) {
      FloatABI = "soft";
    } else if (A->getOption().matches(options::OPT_mhard_float)) {
      FloatABI = "hard";
      DefaultChanged = true;
    } else if (A->getOption().matches(options::OPT_mno_soft_float)) {
      FloatABI = "none";
      DefaultChanged = true;
    } else {
      FloatABI = A->getValue();
      if (FloatABI != "soft" && FloatABI != "hard" &&
          FloatABI != "none" && FloatABI != "simple")
      {
        D.Diag(diag::err_drv_invalid_mfloat_abi) << A->getAsString(Args);
        FloatABI = "soft";
      }
      DefaultChanged = (FloatABI != "soft");
    }
  }

  return FloatABI;
}

void patmos::PatmosBaseTool::ConstructLLCJob(const Tool &Creator,
    Compilation &C, const JobAction &JA,
    const InputInfo &Output, const InputInfoList &Inputs,
    const char *OutputFilename, const char *InputFilename,
    const ArgList &Args, bool EmitAsm) const
{
  ArgStringList LLCArgs;

  bool ChangedFloatABI;
  StringRef FloatABI = getPatmosFloatABI(TC.getDriver(), C.getArgs(),
                                         TC.getTriple(), ChangedFloatABI);

  //----------------------------------------------------------------------------
  // append -O and -m options

  char OptLevel;
  if (Arg* A = GetOptLevel(Args, OptLevel)) {
    switch (OptLevel) {
    case '0':
    case '1':
    case '2':
    case '3':
      A->render(Args, LLCArgs);
      break;
    case '\0':
      // LLC doesn't support -O, uses -O1 instead
      LLCArgs.push_back("-O1");
      break;
    case 's':
    case 'z':
      // LLC does not support -Os, -Oz, uses -O2 instead
      LLCArgs.push_back("-O2");
      break;
    default:
      // LLC doesn't support -Ofast/-O4/-O5.., uses -O3 instead
      LLCArgs.push_back("-O3");
      break;
    }
  } else {
    // If no -O level is supplied, force llc to use -O0
    LLCArgs.push_back("-O0");
  }

  // floating point arguments are different for LLC
  for (ArgList::const_iterator
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_mllvm)) {
      A->claim();
      A->renderAsInput(Args, LLCArgs);
    }
  }

  if (ChangedFloatABI) {
    LLCArgs.push_back("-float-abi");
    LLCArgs.push_back(FloatABI == "hard" ? "hard" : "soft");
  }

  //----------------------------------------------------------------------------
  // generate object file

  // Checking for JA.getType() == types::TY_Image does not tell us if we want to
  // generate asm code since we told clang that assembly files are .bc files
  if (EmitAsm) {
    LLCArgs.push_back("-show-mc-encoding");
  } else {
    LLCArgs.push_back("-filetype=obj");
  }

  //----------------------------------------------------------------------------
  // append output file for code generation

  assert(OutputFilename);
  LLCArgs.push_back("-o");
  LLCArgs.push_back(OutputFilename);

  //----------------------------------------------------------------------------
  // append linked BC name as input

  LLCArgs.push_back(InputFilename);

  const char *LLCExec = Args.MakeArgString(get_patmos_tool(TC, "llc"));
  C.addCommand(std::make_unique<Command>(
      JA, Creator, ResponseFileSupport::AtFileCurCP(),
      LLCExec, LLCArgs, Inputs, Output));
}

static std::string get_patmos_gold(const ToolChain &TC, DiagnosticsEngine &Diag)
{
  char *gold_envvar = getenv("PATMOS_GOLD");
  if (gold_envvar && strcmp(gold_envvar,"")!=0 ) {
    if (llvm::sys::fs::exists(gold_envvar)) {
      return std::string(gold_envvar);
    } else{
      auto DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Error,
                           "gold linker specified through PATMOS_GOLD "
                           "environment variable not found: '%0'");
      Diag.Report(DiagID) << gold_envvar;
      return "";
    }
  }

  std::string longname = TC.getTriple().str() + "-ld";
  std::string tmp( TC.GetProgramPath(longname.c_str()) );
  if (tmp != longname)
    return tmp;

  tmp = TC.GetProgramPath("patmos-ld");
  if (tmp != "patmos-ld")
    return tmp;

  tmp = TC.GetProgramPath("ld.gold");
  if (tmp != "ld.gold")
    return tmp;

  tmp = TC.GetProgramPath("ld-new");
  if (tmp != "ld-new")
    return tmp;

  return TC.GetProgramPath("ld");
}

void patmos::PatmosBaseTool::ConstructGoldJob(const Tool &Creator,
    Compilation &C, const JobAction &JA,
    const InputInfo &Output, const InputInfoList &Inputs,
    const char *OutputFilename, const ArgStringList &GoldInputs,
    const ArgList &Args, bool LinkRelocatable, bool AddStackSymbols) const
{
  ArgStringList LDArgs;

  //----------------------------------------------------------------------------
  // linking options
  Args.AddAllArgs(LDArgs, options::OPT_T_Group);

  Args.AddAllArgs(LDArgs, options::OPT_e);

  LDArgs.push_back("-nostdlib");
  LDArgs.push_back("-static");

  if (LinkRelocatable) {
    // Keep relocations
    LDArgs.push_back("-r");
  } else {
    LDArgs.push_back("-defsym");
    LDArgs.push_back("__heap_start=end");
    LDArgs.push_back("-defsym");
    LDArgs.push_back("__heap_end=0x100000");
    if (AddStackSymbols) {
      LDArgs.push_back("-defsym");
      LDArgs.push_back("_shadow_stack_base=0x1f8000");
      LDArgs.push_back("-defsym");
      LDArgs.push_back("_stack_cache_base=0x200000");
    }
  }

  // Do not append arguments given from the Commandline before
  // setting the defaults
  for (ArgList::const_iterator
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_Xlinker)) {
      A->claim();
      A->renderAsInput(Args, LDArgs);
    }
  }

  if (Args.hasArg(options::OPT_v))
    LDArgs.push_back("-verbose");

  //----------------------------------------------------------------------------
  // append output file for code generation

  LDArgs.push_back("-o");
  LDArgs.push_back(OutputFilename);

  //----------------------------------------------------------------------------
  // append all linker input arguments and construct the link command

  LDArgs.append(GoldInputs.begin(), GoldInputs.end());

  const char *LDExec = Args.MakeArgString(get_patmos_gold(TC, C.getDriver().getDiags()));
  C.addCommand(std::make_unique<Command>(
      JA, Creator, ResponseFileSupport::AtFileCurCP(),
      LDExec, LDArgs, Inputs, Output));
}

void patmos::Compile::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs,
                               const ArgList &Args,
                               const char *LinkingOutput) const
{
  bool EmitLLVM = false;
  // only used when EmitLLVM is false
  bool EmitAsm = false;
  // do we have a link phase where we call llc
  bool IsLastPhase = false;

  if (Arg *A = C.getArgs().getLastArg(options::OPT_emit_llvm, options::OPT_S))
  {
    if (A->getOption().matches(options::OPT_emit_llvm))
    {
      EmitLLVM = true;
    } else {
      EmitAsm = true;
      IsLastPhase = true;
    }
    A->claim();
  }
  if (C.getArgs().hasArg(options::OPT_c))
  {
    EmitAsm = false;
    IsLastPhase = true;
  }

  // TODO instead of running LLC separately, we might also just add the opt/LLC
  //      options to the clang options using -mllvm for -Xopt/-Xllc options and
  //      let clang -cc1 do the work (but this is slightly harder to debug).

  // If this is not the last phase or if we emit llvm-code, we just call clang
  if (EmitLLVM || !IsLastPhase) {

    Clang::ConstructJob(C, JA, Output, Inputs, Args, LinkingOutput);

  } else {
    // Run llc afterwards, no linking phase
    const char *BCFilename = CreateOutputFilename(C, Output, "clang-",
                                                  "bc", false);

    const InputInfo TmpOutput(types::TY_LLVM_BC, BCFilename,
                              Inputs[0].getFilename());

    //--------------------------------------------------------------------------
    // Run clang

    Clang::ConstructJob(C, JA, TmpOutput, Inputs, Args, LinkingOutput);

    //--------------------------------------------------------------------------
    // Run LLC on output

    ConstructLLCJob(*this, C, JA, Output, Inputs, Output.getFilename(), BCFilename, Args,
                    EmitAsm);

  }

}

void patmos::Linker::ConstructJob(
    Compilation &C, const JobAction &JA, const InputInfo &Output,
    const InputInfoList &Inputs, const ArgList &Args,
    const char *LinkingOutput) const
{
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();

  // In the link phase, we do the following:
  // - run llvm-link, link in bitcode files:
  //    - startup files if not -nostartfiles
  //    - all bitcode input files
  //    - all -l<library>, except when linking as object file
  //    - all standard libraries, except when disabled
  //    - all runtime libs, except when disabled
  //    - all required libsyms.o files for linked in libs, except when disabled
  // - run llc on result, perform optimization on linked bitcode
  // - run gold on result, link in:
  //    - all ELF input files, same order as llvm-link

  //----------------------------------------------------------------------------
  // read out various command line options

  bool LinkRTEMS = (TC.getTriple().getOS() == llvm::Triple::RTEMS);

  bool ChangedFloatABI;
  StringRef FloatABI = getPatmosFloatABI(TC.getDriver(), C.getArgs(),
                                         TC.getTriple(), ChangedFloatABI);

  // Emit assembly code
  bool EmitAsm = C.getArgs().hasArg(options::OPT_S);
  // Emit an object file, not an executable.
  bool EmitObject = C.getArgs().hasArg(options::OPT_C) || EmitAsm;
  // Emit LLVM-IR
  bool EmitLLVM = C.getArgs().hasArg(options::OPT_emit_llvm);

  // add lib*syms.o options to llvm-link
  bool AddLibSyms = !EmitObject;
  // add crt0
  bool AddStartFiles = !EmitObject;
  // add librt, librtsf
  bool AddRuntimeLibs = !EmitObject;
  // add libpatmos, libc, ..
  bool AddStdLibs = !EmitObject;
  // add libc
  bool AddLibC = !EmitObject;
  // add support library for newlib libc
  bool AddLibGloss = AddStdLibs && !LinkRTEMS;
  // add any default libs at all?
  bool AddDefaultLibs = !EmitObject;

  if (Arg *A = Args.getLastArg(options::OPT_shared)) {
    D.Diag(diag::err_drv_unsupported_opt) << A->getAsString(Args);
  }
  if (Arg *A = Args.getLastArg(options::OPT_static)) {
    D.Diag(diag::err_drv_unsupported_opt) << A->getAsString(Args);
  }


  //////////////////////////////////////////////////////////////////////////////
  // Prepare linker arguments

  const char *linkedBCFileName =
           CreateOutputFilename(C, Output, "link-", "bc", EmitLLVM);

  ArgStringList LinkInputs, GoldInputs;
  unsigned linkedOFileInsertPos;

  bool HasGoldPass = !EmitLLVM;

  const char *BCFile = PrepareLinkerInputs(Args, Inputs,
                         LinkInputs, GoldInputs,
                         linkedBCFileName, linkedOFileInsertPos,
                         AddStartFiles, AddRuntimeLibs, AddLibGloss, AddLibC,
                         AddLibSyms, FloatABI,
                         AddDefaultLibs, HasGoldPass, false);

  bool RequiresLink = true;
  if ((!BCFile || BCFile != linkedBCFileName) && !EmitLLVM) {
    // No bitcode input, or only a single bitcode input file, skip link
    // pass and use input directly if it is a single file.
    RequiresLink = false;
    linkedBCFileName = BCFile;
  }

  //////////////////////////////////////////////////////////////////////////////
  // build LINK command

  if (RequiresLink) {
    ConstructLinkJob(*this, C, JA, Output, Inputs, linkedBCFileName, LinkInputs, Args);
  }

  // Stop after LLVM-link
  if (EmitLLVM) {
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // build OPT command

  bool SkipOpt = C.getArgs().hasArg(options::OPT_O0);
  if (!SkipOpt && linkedBCFileName) {
    char const *optimizedBCFileName =
                CreateOutputFilename(C, Output, "opt-", "opt.bc", false);

    if (ConstructOptJob(
          *this, C, JA, Output, Inputs, optimizedBCFileName,
          linkedBCFileName,  Args, true, false ))
    {
      linkedBCFileName = optimizedBCFileName;
    }
  }

  // If we only want to emit bitcode, we are done now.
  if (EmitLLVM) {
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // build LLC command

  const char *linkedOFileName = EmitLLVM ? 0 :
           CreateOutputFilename(C, Output, "llc-", "bc.o", false);

  if (linkedBCFileName) {
    ConstructLLCJob(*this, C, JA, Output, Inputs,
        linkedOFileName, linkedBCFileName, Args,  EmitAsm);
  }

  // If we do not want to create an executable file, we are done now
  if (EmitObject) {
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // build LD command

  char const *linkedELFFileName = CreateOutputFilename(C, Output, "gold-",
                                                       ".out", true);

  if (linkedBCFileName) {
    // If we compiled a bitcode file, insert the compiled file into gold args
    GoldInputs.insert(GoldInputs.begin() + linkedOFileInsertPos,
                      linkedOFileName);
  }
  bool LinkRelocatable = C.getArgs().hasArg(options::OPT_c);
  ConstructGoldJob(*this, C, JA, Output, Inputs, linkedELFFileName,
      GoldInputs, Args, LinkRelocatable, !LinkRTEMS);
}

void patmos::getPatmosTargetFeatures(const Driver &D, const llvm::Triple &Triple,
                            const llvm::opt::ArgList &Args,
                            std::vector<llvm::StringRef> &Features)
{
  // No target specific features
}
StringRef patmos::getPatmosABI(const llvm::opt::ArgList &Args,
                      const llvm::Triple &Triple)
{
  assert(Triple.getArch() == llvm::Triple::patmos  &&
         "Unexpected triple");
  return ""; // We have no name for our ABI
}
StringRef patmos::getPatmosArch(const llvm::opt::ArgList &Args,
                       const llvm::Triple &Triple)
{
  assert(Triple.getArch() == llvm::Triple::patmos  &&
         "Unexpected triple");
  return "patmos";
}

// Patmos tools end.
