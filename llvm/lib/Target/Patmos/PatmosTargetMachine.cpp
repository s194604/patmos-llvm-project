//===-- PatmosTargetMachine.cpp - Define TargetMachine for Patmos ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Top-level implementation for the Patmos target.
//
//===----------------------------------------------------------------------===//

#include "Patmos.h"
#include "PatmosTargetMachine.h"
#include "SinglePath/PatmosSinglePathInfo.h"
#include "PatmosSchedStrategy.h"
#include "PatmosStackCacheAnalysis.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/Transforms/Utils.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePatmosTarget() {
  // Register the target.
  RegisterTargetMachine<PatmosTargetMachine> X(getThePatmosTarget());
}

static ScheduleDAGInstrs *createPatmosVLIWMachineSched(MachineSchedContext *C) {
  // TODO instead of the generic ScheduleDAGMI, we might want to use a different
  // scheduler that allows for VLIW bundling or something, similar to the
  // PatmosPostRAScheduler. Should still be split into a generic ScheduleDAG
  // scheduler and a specialised PatmosSchedStrategy.
  ScheduleDAGMI *PS = new ScheduleDAGMI(C, std::make_unique<PatmosVLIWSchedStrategy>(), false);
  return PS;
}

static MachineSchedRegistry
SchedCustomRegistry("patmos", "Run Patmos's custom scheduler",
                    createPatmosVLIWMachineSched);


namespace {
  /// EnableStackCacheAnalysis - Option to enable the analysis of Patmos' stack
  /// cache usage.
  static cl::opt<bool> EnableStackCacheAnalysis(
    "mpatmos-enable-stack-cache-analysis",
    cl::init(false),
    cl::desc("Enable the Patmos stack cache analysis."),
    cl::Hidden);
  static cl::opt<bool> DisableIfConverter(
      "mpatmos-disable-ifcvt",
      cl::init(false),
      cl::desc("Disable if-converter for Patmos."),
      cl::Hidden);

  /// Patmos Code Generator Pass Configuration Options.
  class PatmosPassConfig : public TargetPassConfig {
  private:
    const std::string DefaultRoot;

  public:
    PatmosPassConfig(PatmosTargetMachine &TM, PassManagerBase &PM)
     : TargetPassConfig(TM, PM), DefaultRoot("main")
    {
      // Enable preRA MI scheduler.
      if (TM.getSubtargetImpl()->usePreRAMIScheduler(getOptLevel())) {
        enablePass(&MachineSchedulerID);
      }
      if (TM.getSubtargetImpl()->usePatmosPostRAScheduler(getOptLevel())) {
        initializePatmosPostRASchedulerPass(*PassRegistry::getPassRegistry());
        substitutePass(&PostRASchedulerID, &PatmosPostRASchedulerID);
      }
    }

    PatmosTargetMachine &getPatmosTargetMachine() const {
      return getTM<PatmosTargetMachine>();
    }

    const PatmosSubtarget &getPatmosSubtarget() const {
      return *getPatmosTargetMachine().getSubtargetImpl();
    }

    ScheduleDAGInstrs *
    createMachineScheduler(MachineSchedContext *C) const override {
      return createPatmosVLIWMachineSched(C);
    }

    ScheduleDAGInstrs *
    createPostMachineScheduler(MachineSchedContext *C) const override {
      llvm_unreachable("unimplemented");
    }

    bool addInstSelector() override {
      addPass(createPatmosISelDag(getPatmosTargetMachine(), getOptLevel()));
      return false;
    }

    //
    /// addPreISelPasses - This method should add any "last minute" LLVM->LLVM
    /// passes (which are run just before instruction selector).
    bool addPreISel() override {
      if (PatmosSinglePathInfo::isEnabled()) {
        // Single-path transformation requires a single exit node
        addPass(createUnifyFunctionExitNodesPass());
        // Single-path transformation currently cannot deal with
        // switch/jumptables -> lower them to ITEs
        addPass(createLowerSwitchPass());
        addPass(createPatmosSPClonePass());
        return true;
      }
      return false;
    }

    /// addPreRegAlloc - This method may be implemented by targets that want to
    /// run passes immediately before register allocation. This should return
    /// true if -print-machineinstrs should print after these passes.
    void addPreRegAlloc() override {
      // For -O0, add a pass that removes dead instructions to avoid issues
      // with spill code in naked functions containing function calls with
      // unused return values.
      if (getOptLevel() == CodeGenOpt::None) {
        addPass(&DeadMachineInstructionElimID);
      }
    }

    /// addPostRegAlloc - This method may be implemented by targets that want to
    /// run passes after register allocation pass pipeline but before
    /// prolog-epilog insertion.  This should return true if -print-machineinstrs
    /// should print after these passes.
    void addPostRegAlloc() override {
      if (PatmosSinglePathInfo::isEnabled()) {
        addPass(createPatmosSPMarkPass(getPatmosTargetMachine()));
        addPass(createPatmosSinglePathInfoPass(getPatmosTargetMachine()));
        addPass(createPatmosSPPreparePass(getPatmosTargetMachine()));
      }
    }


    /// addPreSched2 - This method may be implemented by targets that want to
    /// run passes after prolog-epilog insertion and before the second instruction
    /// scheduling pass.  This should return true if -print-machineinstrs should
    /// print after these passes.
    void addPreSched2() override {

      if (PatmosSinglePathInfo::isEnabled()) {
        addPass(createPatmosSinglePathInfoPass(getPatmosTargetMachine()));
        addPass(createPatmosSPBundlingPass(getPatmosTargetMachine()));
        addPass(createPatmosSPReducePass(getPatmosTargetMachine()));
        addPass(createSPSchedulerPass(getPatmosTargetMachine()));
      } else {
        if (getOptLevel() != CodeGenOpt::None && !DisableIfConverter) {
          addPass(&IfConverterID);
          // If-converter might create unreachable blocks (bug?), need to be
          // removed before function splitter
          addPass(&UnreachableMachineBlockElimID);
        }
      }

      // this is pseudo pass that may hold results from SC analysis
      // (currently for PML export)
      addPass(createPatmosStackCacheAnalysisInfo(getPatmosTargetMachine()));

      if (EnableStackCacheAnalysis) {
        addPass(createPatmosStackCacheAnalysis(getPatmosTargetMachine()));
      }
    }


    void addBlockPlacement() override {
      // We leave this function empty to avoid LLVM's default
      // probabilistic block placement pass, as it seems to
      // mess with terminating instructions on some blocks,
      // resulting in invalid branch targets.
    }

    /// addPreEmitPass - This pass may be implemented by targets that want to run
    /// passes immediately before machine code is emitted.  This should return
    /// true if -print-machineinstrs should print out the code after the passes.
    void addPreEmitPass() override {

      // Post-RA MI Scheduler does bundling and delay slots itself. Otherwise,
      // add passes to handle them.
      if (!getPatmosSubtarget().usePatmosPostRAScheduler(getOptLevel())) {
        addPass(createPatmosDelaySlotFillerPass(getPatmosTargetMachine(),
                                            getOptLevel() == CodeGenOpt::None));
      }

      // All passes below this line must handle delay slots and bundles
      // correctly.

      if (getPatmosSubtarget().hasMethodCache()) {
        addPass(createPatmosFunctionSplitterPass(getPatmosTargetMachine()));
      }

      addPass(createPatmosDelaySlotKillerPass(getPatmosTargetMachine()));

      addPass(createPatmosEnsureAlignmentPass(getPatmosTargetMachine()));
    }
  };
} // namespace

static Reloc::Model getEffectiveRelocModel(bool JIT,
                                           Optional<Reloc::Model> RM) {
  if (!RM.hasValue() || JIT)
    return Reloc::Static;
  return *RM;
}

PatmosTargetMachine::PatmosTargetMachine(const Target &T,
                                         const Triple &TT,
                                         StringRef CPU,
                                         StringRef FS,
                                         const TargetOptions &Options,
                                         Optional<Reloc::Model> RM,
                                         Optional<CodeModel::Model> CM,
                                         CodeGenOpt::Level L, bool JIT)
  : LLVMTargetMachine(
      T,
      // Keep this in sync with clang/lib/Basic/Targets.cpp and
      // compiler-rt/lib/patmos/*.ll
      // Note: Both ABI and Preferred Alignment must be 32bit for all supported
      // types, backend does not support different stack alignment.
      "E-S32-p:32:32:32-i8:8:8-i16:16:16-i32:32:32-i64:32:32-f64:32:32-a0:0:32-s0:32:32-v64:32:32-v128:32:32-n32",
      TT, CPU, FS, Options, getEffectiveRelocModel(JIT, RM), getEffectiveCodeModel(CM, CodeModel::Small), L),
    Subtarget(TT, CPU, FS, *this, L), InstrInfo(*this), TSInfo(), TLOF(std::make_unique<PatmosTargetObjectFile>())
{
  initAsmInfo();
}

TargetPassConfig *PatmosTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new PatmosPassConfig(*this, PM);
}
