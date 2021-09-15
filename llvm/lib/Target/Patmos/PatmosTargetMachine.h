//==-- PatmosTargetMachine.h - Define TargetMachine for Patmos ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the Patmos specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//


#ifndef _LLVM_TARGET_PATMOS_TARGETMACHINE_H_
#define _LLVM_TARGET_PATMOS_TARGETMACHINE_H_

#include "PatmosInstrInfo.h"
#include "PatmosISelLowering.h"
#include "PatmosFrameLowering.h"
#include "PatmosSelectionDAGInfo.h"
#include "PatmosRegisterInfo.h"
#include "PatmosSubtarget.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

/// PatmosTargetMachine
///
class PatmosTargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  PatmosSubtarget        Subtarget;
  PatmosInstrInfo        InstrInfo;
  PatmosSelectionDAGInfo TSInfo;

public:
  PatmosTargetMachine(const Target &T,
                      const Triple &TT,
                      StringRef CPU,
                      StringRef FS,
                      const TargetOptions &Options,
                      Optional<Reloc::Model> RM,
                      Optional<CodeModel::Model> CM,
                      CodeGenOpt::Level L, bool JIT);

  const PatmosInstrInfo *getInstrInfo() const { return &InstrInfo; }
  const DataLayout *getDataLayout() const { return &DL;}
  const PatmosSubtarget *getSubtargetImpl(const Function &F) const override{
    return getSubtargetImpl();
  }
  const PatmosSubtarget *getSubtargetImpl() const { return &Subtarget; }

  const TargetRegisterInfo *getRegisterInfo() const {
    return &InstrInfo.getRegisterInfo();
  }

  const PatmosSelectionDAGInfo* getSelectionDAGInfo() const {
    return &TSInfo;
  }

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  const InstrItineraryData *getInstrItineraryData() const {  return Subtarget.getInstrItineraryData(); }

  /// createPassConfig - Create a pass configuration object to be used by
  /// addPassToEmitX methods for generating a pipeline of CodeGen passes.
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

}; // PatmosTargetMachine.

} // end namespace llvm

#endif // _LLVM_TARGET_PATMOS_TARGETMACHINE_H_
