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
  if (TripleString == "patmos") {
    TripleString = "patmos-unknown-unknown-elf";
  }

  // newlib libraries and includes
  if (llvm::sys::fs::exists(Path + "/" + TripleString + "/")){
    getFilePaths().push_back(Path + "/" + TripleString + "/");
  }
  if (llvm::sys::fs::exists(Path + "/../" + TripleString + "/")){
    getFilePaths().push_back(Path + "/../" + TripleString + "/");
  }
}

void PatmosToolChain::AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                              llvm::opt::ArgStringList &CC1Args) const
{
  // We don't want actual system includes to be used as they will never be useful
  // E.g. "/usr/local/include" or "/usr/include"
  CC1Args.push_back("-nostdsysteminc");

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

static bool matchesJob(const Action &action, types::ID t, Action::ActionClass a) {
  return action.getType() == t && action.getKind() == a;
}

Tool *PatmosToolChain::SelectTool(const JobAction &JA) const {
  if( matchesJob(JA, types::TY_LLVM_BC, Action::CompileJobClass) ){
    return getPatmosCompile();
  }
  if( matchesJob(JA, types::TY_Image, Action::LinkJobClass) ){
    return getPatmosFinalLink();
  }
  auto &Diag = getDriver().getDiags();
  auto DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Error,
                       "Patmos tool selector: Unknown job: type '%0', kind '%1'");
  Diag.Report(DiagID) << getTypeName(JA.getType()) << JA.getKind();
  return NULL;
}

Tool *PatmosToolChain::getPatmosCompile() const {
  if (!Compile)
    Compile.reset(new tools::patmos::Compile(*this));
  return Compile.get();
}
Tool *PatmosToolChain::getPatmosFinalLink() const {
  if (!FinalLink)
    FinalLink.reset(new tools::patmos::FinalLink(*this));
  return FinalLink.get();
}

const char * patmos::PatmosBaseTool::CreateOutputFilename(Compilation &C,
    const InputInfo &Output, const char * TmpPrefix, const char *Suffix,
    bool IsLastPass) const
{
  const char * filename = NULL;

  const ArgList &Args = C.getArgs();

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
      std::string TmpName = TC.getDriver().GetTemporaryPath(Split.first, Suffix);
      filename = Args.MakeArgString(TmpName.c_str());
      C.addTempFile(filename);
    }
  }
  return filename;
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

std::string patmos::PatmosBaseTool::getLibPath(const char* LibName) const {
  auto path = TC.GetFilePath(LibName);
  if (!llvm::sys::fs::exists(path)) {
    auto DiagID = TC.getDriver().getDiags().getCustomDiagID(DiagnosticsEngine::Error,
                               "couldn't find library file: '%0'");
    TC.getDriver().getDiags().Report(DiagID) << path;
    return LibName;
  }
  return path;
}

void patmos::PatmosBaseTool::PrepareLink1Inputs(
    const llvm::opt::ArgList &Args,
    const InputInfoList &Inputs,
    llvm::opt::ArgStringList &LinkInputs) const
{
  for (InputInfoList::const_iterator
          it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
     const InputInfo &II = *it;

     if (II.isFilename()) {
       LinkInputs.push_back(II.getFilename());
     }
  }

  if(Args.hasArg(options::OPT_v)) {
    LinkInputs.push_back("-v");
  }
}

void patmos::PatmosBaseTool::PrepareLink2Inputs(
    const llvm::opt::ArgList &Args,
    const char* Input,
    llvm::opt::ArgStringList &LinkInputs) const
{
  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/crt0.o")));
  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/crtbegin.o")));
  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/crtend.o")));

  LinkInputs.push_back(Input);

  // We hide symbols to allow redefinition of stdlib symbols without
  // clashing with stdlib
  LinkInputs.push_back("--internalize");

  // Don't hide symbols that are expected to be public
  LinkInputs.push_back(Args.MakeArgString("--internalize-public-api-file=" + getLibPath("lib/libsyms.lst")));

  if(Args.hasArg(options::OPT_v)) {
    LinkInputs.push_back("-v");
  }
}

void patmos::PatmosBaseTool::PrepareLink3Inputs(
    const llvm::opt::ArgList &Args,
    const char* Input,
    llvm::opt::ArgStringList &LinkInputs) const
{
    LinkInputs.push_back(Input);
  //----------------------------------------------------------------------------
  // link with newlib and compiler-rt libraries

  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/libc.a")));
  LinkInputs.push_back(Args.MakeArgString("--override=" + getLibPath("lib/libm.a")));
  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/libpatmos.a")));

  LinkInputs.push_back(Args.MakeArgString("--internalize-public-api-file=" + getLibPath("lib/libsyms.lst")));

  if(Args.hasArg(options::OPT_v)) {
    LinkInputs.push_back("-v");
  }
}

void patmos::PatmosBaseTool::PrepareLink4Inputs(
    const llvm::opt::ArgList &Args,
    const char* Input,
    llvm::opt::ArgStringList &LinkInputs) const
{
  LinkInputs.push_back(Input);

  // All of compiler-rt must always be available
  LinkInputs.push_back(Args.MakeArgString(getLibPath("lib/librt.a")));

  if(Args.hasArg(options::OPT_v)) {
    LinkInputs.push_back("-v");
  }
}

static std::string get_patmos_tool(const ToolChain &TC, StringRef ToolName)
{
  std::string InstallPath(TC.getDriver().getInstalledDir());
  if(TC.getDriver().Name.rfind("patmos-", 0) == 0) {
    // Driver is named "patmos-xxxx"
    // therefore, look for programs using that prefix
    std::string longname = (TC.getTriple().str() + "-" + ToolName).str();
    std::string tmp(TC.GetProgramPath(longname.c_str()) );
    if (tmp != longname)
      return tmp;

    std::string PatmosTool = ("patmos-" + ToolName).str();
    tmp = TC.GetProgramPath(PatmosTool.c_str());
    if (tmp != PatmosTool)
      return tmp;

  } else if ( llvm::sys::fs::exists(InstallPath + "/" + ToolName)){
    return (InstallPath + "/" + (ToolName)).str();
  }
  // Couldn't find anything
  return ToolName.str();
}

void patmos::PatmosBaseTool::ConstructLLVMLinkJob(const Tool &Creator,
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

  for (ArgList::const_iterator
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_Xlinker)) {
      A->claim();
      A->renderAsInput(Args, CmdArgs);
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
                     const ArgList &Args) const
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
  if(OptLevel == 0) {
    return false;
  }
  //----------------------------------------------------------------------------
  // append optimization options

  // pass -O level to opt verbatim
  OptArg->renderAsInput(Args, OptArgs);

  // for some reason, we need to add this manually
  OptArgs.push_back("--internalize");
  OptArgs.push_back("--globaldce");
  OptArgs.push_back(Args.MakeArgString("--internalize-public-api-file=" + getLibPath("lib/libsyms.lst")));


  if (OptLevel == 3) {
    OptArgs.push_back("--std-link-opts");
  }

  //----------------------------------------------------------------------------
  // append output and input files

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

void patmos::PatmosBaseTool::ConstructLLCJob(const Tool &Creator,
    Compilation &C, const JobAction &JA,
    const InputInfo &Output, const InputInfoList &Inputs,
    const char *OutputFilename, const char *InputFilename,
    const ArgList &Args) const
{
  ArgStringList LLCArgs;

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

  //----------------------------------------------------------------------------
  // generate object file

  LLCArgs.push_back("-filetype=obj");

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

  auto tmp = TC.GetProgramPath("patmos-ld");
  if (tmp != "patmos-ld") {
    return tmp;
  }else{
    auto DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Error,
                         "gold linker 'patmos-ld' not found");
    Diag.Report(DiagID);
    return "";
  }
}

void patmos::PatmosBaseTool::ConstructGoldJob(const Tool &Creator,
    Compilation &C, const JobAction &JA,
    const InputInfo &Output, const InputInfoList &Inputs,
    const char *OutputFilename, const ArgStringList &GoldInputs,
    const ArgList &Args, bool AddStackSymbols) const
{
  ArgStringList LDArgs;

  //----------------------------------------------------------------------------
  // linking options
  Args.AddAllArgs(LDArgs, options::OPT_T_Group);

  Args.AddAllArgs(LDArgs, options::OPT_e);

  LDArgs.push_back("-nostdlib");
  LDArgs.push_back("-static");

  LDArgs.push_back("--defsym");
  LDArgs.push_back("__heap_start=end");
  LDArgs.push_back("--defsym");
  LDArgs.push_back("__heap_end=0x100000");
  if (AddStackSymbols) {
    LDArgs.push_back("--defsym");
    LDArgs.push_back("_shadow_stack_base=0x1f8000");
    LDArgs.push_back("--defsym");
    LDArgs.push_back("_stack_cache_base=0x200000");
  }

  // Do not append arguments given from the Commandline before
  // setting the defaults
  for (ArgList::const_iterator
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_Wl_COMMA)) {
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
  if( // Compile to bitcode
      matchesJob(JA, types::TY_LLVM_BC, Action::BackendJobClass) ||
      // Compile to LLVM-IR (textual format)
      matchesJob(JA, types::TY_LLVM_IR, Action::BackendJobClass) ||
      // Compile to assembly (textual format)
      matchesJob(JA, types::TY_PP_Asm, Action::BackendJobClass)
  ) {
    Clang::ConstructJob(C, JA, Output, Inputs, Args, LinkingOutput);
  } else if(
      // Compile to machine code
      matchesJob(JA, types::TY_Object, Action::AssembleJobClass)
  ){
    // Make job to compile to BC
    BackendJobAction prelink_job((Action*) &JA, types::TY_LLVM_BC);

    if( C.getActions().size() > 0 &&
        matchesJob(**C.getActions().begin(), types::TY_Image, Action::LinkJobClass)
    ){
      // The ultimate job is to produce an executable, therefore, produce only bitcode
      // which is compiled into machine code in FinalLink
      Clang::ConstructJob(C, prelink_job, Output, Inputs, Args, LinkingOutput);
    } else {
      // We just need an object file
      const char *BCFilename = CreateOutputFilename(C, Output, "clang-", "bc", false);

      const InputInfo TmpOutput(types::TY_LLVM_BC, BCFilename, Inputs[0].getFilename());
      Clang::ConstructJob(C, prelink_job, TmpOutput, Inputs, Args, LinkingOutput);

      ////////////////////////////////////////////////////////////////////////////
      // build LLC command
      ConstructLLCJob(*this, C, JA, Output, Inputs, Output.getFilename(),
          BCFilename, Args);
    }

  } else {
    auto &Diag = TC.getDriver().getDiags();
    auto DiagID = Diag.getCustomDiagID(DiagnosticsEngine::Error,
                         "Patmos CompileBC: Unknown job: type '%0', kind '%1'");
    Diag.Report(DiagID) << getTypeName(JA.getType()) << JA.getKind();
  }
}

void patmos::FinalLink::ConstructJob(Compilation &C, const JobAction &JA,
                               const InputInfo &Output,
                               const InputInfoList &Inputs,
                               const ArgList &Args,
                               const char *LinkingOutput) const
{
  //////////////////////////////////////////////////////////////////////////////
  // build LINK 1 command
  const char *link1Out = CreateOutputFilename(C, Output, "link-", "bc", false);
  ArgStringList LinkInputs;
  PrepareLink1Inputs(Args, Inputs, LinkInputs);
  ConstructLLVMLinkJob(*this, C, JA, Output, Inputs, link1Out, LinkInputs, Args);

  //////////////////////////////////////////////////////////////////////////////
  // build LINK 2 command
  const char *link2Out = CreateOutputFilename(C, Output, "link12-", "bc", false);

  ArgStringList Link2Inputs;
  PrepareLink2Inputs(Args, link1Out, Link2Inputs);
  ConstructLLVMLinkJob(*this, C, JA, Output, Inputs, link2Out, Link2Inputs, Args);

  //////////////////////////////////////////////////////////////////////////////
  // build LINK 3 command
  const char *link3Out = CreateOutputFilename(C, Output, "link2-", "bc", false);

  ArgStringList Link3Inputs;
  PrepareLink3Inputs(Args, link2Out, Link3Inputs);
  ConstructLLVMLinkJob(*this, C, JA, Output, Inputs, link3Out, Link3Inputs, Args);

  //////////////////////////////////////////////////////////////////////////////
  // build OPT command

  const char* optOut = CreateOutputFilename(C, Output, "opt-", "opt.bc", false);
  if (!ConstructOptJob( *this, C, JA, Output, Inputs, optOut, link3Out,  Args)) {
    optOut = link3Out;
  }

  //////////////////////////////////////////////////////////////////////////////
  // build LINK 4
  const char *link4Out = CreateOutputFilename(C, Output, "link3-", "bc", false);

  ArgStringList Link4Inputs;
  PrepareLink4Inputs(Args, optOut, Link4Inputs);
  ConstructLLVMLinkJob(*this, C, JA, Output, Inputs, link4Out, Link4Inputs, Args);

  ////////////////////////////////////////////////////////////////////////////
  // build LLC command
  const char *llcOut = CreateOutputFilename(C, Output, "llc-", "bc", false);
  ConstructLLCJob(*this, C, JA, Output, Inputs, llcOut,
                  link4Out, Args);

  ArgStringList GoldInputs;
  GoldInputs.push_back(llcOut);
  ConstructGoldJob(*this, C, JA, Output, Inputs, Output.getFilename(),
      GoldInputs, Args, true);

}

