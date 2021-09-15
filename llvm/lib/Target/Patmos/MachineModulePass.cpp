//===-- MachineModulePass.cpp -------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the definitions of the MachineModulePass members.
//
//===----------------------------------------------------------------------===//

#include "MachineModulePass.h"
#include "llvm/IR/LegacyPassManagers.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Module.h"

using namespace llvm;

bool MachineModulePass::runOnModule(Module &M) {
  bool Changed = runOnMachineModule(M);
  return Changed;
}

void MachineModulePass::getAnalysisUsage(AnalysisUsage &AU) const {
  // MachineModulePass preserves all LLVM IR passes, but there's no
  // high-level way to express this. Instead, just list a bunch of
  // passes explicitly. This does not include setPreservesCFG,
  // because CodeGen overloads that to mean preserving the MachineBasicBlock
  // CFG in addition to the LLVM IR CFG.
  AU.addPreserved<AAResultsWrapperPass>();
  AU.addPreserved("scalar-evolution");
  AU.addPreserved("iv-users");
  AU.addPreserved("memdep");
  AU.addPreserved("live-values");
  AU.addPreserved("domtree");
  AU.addPreserved("domfrontier");
  AU.addPreserved("loops");
  AU.addPreserved("lda");

  ModulePass::getAnalysisUsage(AU);
}
