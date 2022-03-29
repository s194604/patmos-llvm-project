#ifndef TARGET_PATMOS_SINGLEPATH_PATMOSSPBUNDLING_H_
#define TARGET_PATMOS_SINGLEPATH_PATMOSSPBUNDLING_H_

#include "Patmos.h"
#include "PatmosMachineFunctionInfo.h"
#include "PatmosSinglePathInfo.h"
#include "PatmosTargetMachine.h"
#include "PatmosSinglePathInfo.h"
#include "llvm/IR/Metadata.h"
#include "llvm/CodeGen/MachinePostDominators.h"

#define DEBUG_TYPE "patmos-singlepath"

namespace llvm {

class PatmosSPBundling : public MachineFunctionPass {

private:

  const PatmosTargetMachine &TM;
  const PatmosSubtarget &STC;
  const PatmosInstrInfo *TII;
  const PatmosRegisterInfo *TRI;

  PatmosSinglePathInfo *PSPI;

  MachinePostDominatorTree *PostDom;

  /// doBundlingFunction - Bundle a given MachineFunction
  void doBundlingFunction(SPScope* root);

public:
  static char ID;
  PatmosSPBundling(const PatmosTargetMachine &tm):
       MachineFunctionPass(ID), TM(tm),
       STC(*tm.getSubtargetImpl()),
       TII(static_cast<const PatmosInstrInfo*>(tm.getInstrInfo())),
       TRI(static_cast<const PatmosRegisterInfo*>(tm.getRegisterInfo()))
  {}

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos Single-Path Bundling (machine code)";
  }

  /// getAnalysisUsage - Specify which passes this pass depends on
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<PatmosSinglePathInfo>();
    AU.addRequired<MachinePostDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool doInitialization(Module &M) override {
    return false;
  }

  bool doFinalization(Module &M) override {
    return false;
  }

  bool runOnMachineFunction(MachineFunction &MF) override ;

  SPScope* getRootScope(){
    return PSPI->getRootScope();
  }

  /// Tries to find a pair of blocks to merge.
  ///
  /// If a pair is found, true is returned with the pair of blocks found.
  /// If no pair is found, false is returned with a pair of NULLs.
  std::pair<bool, std::pair<PredicatedBlock*,PredicatedBlock*>>
  findMergePair(const SPScope*);

  void mergeMBBs(MachineBasicBlock *mbb1, MachineBasicBlock *mbb2);

  void bundleScope(SPScope* root);
};

}

#endif /* TARGET_PATMOS_SINGLEPATH_PATMOSSPBUNDLING_H_ */
