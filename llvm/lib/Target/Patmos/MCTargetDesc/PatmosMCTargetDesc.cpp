//===-- PatmosMCTargetDesc.cpp - Patmos Target Descriptions -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides Patmos specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "PatmosMCTargetDesc.h"
#include "PatmosMCAsmInfo.h"
#include "PatmosTargetStreamer.h"
#include "InstPrinter/PatmosInstPrinter.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#include "PatmosGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "PatmosGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "PatmosGenRegisterInfo.inc"

using namespace llvm;

static MCAsmInfo *createPatmosMCAsmInfo(const MCRegisterInfo &MRI,
                                        const Triple &TT,
                                        const MCTargetOptions &Options)
{
  MCAsmInfo *MAI = new PatmosMCAsmInfo(TT);

  unsigned SP = MRI.getDwarfRegNum(Patmos::RSP, true);
  MCCFIInstruction Inst = MCCFIInstruction::cfiDefCfa(nullptr, SP, 0);
  MAI->addInitialFrameState(Inst);

  return MAI;
}

static MCInstrInfo *createPatmosMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitPatmosMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createPatmosMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitPatmosMCRegisterInfo(X, Patmos::R1);
  return X;
}

static MCSubtargetInfo *
createPatmosMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  if (CPU.empty()) {
    // TODO since we do not create a PatmosSubtarget here (for some registration
    // issues it seems), we need to take care about the default CPU model here
    // as well, otherwise we have no SchedModel.
    CPU = "generic";
  }
  return createPatmosMCSubtargetInfoImpl(TT, CPU, FS);
}

static MCInstPrinter *createPatmosMCInstPrinter(const Triple &T,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  return new PatmosInstPrinter(MAI, MII, MRI);
}

static MCTargetStreamer *createPatmosAsmTargetStreamer(MCStreamer &S,
                                                       formatted_raw_ostream &OS,
                                                       MCInstPrinter *InstPrint,
                                                       bool isVerboseAsm) {
  return new PatmosTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *
createPatmosObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new PatmosTargetELFStreamer(S);
}

bool llvm::canIssueInSlotForUnits(unsigned Slot, llvm::InstrStage::FuncUnits &units)
{
  switch (Slot) {
  case 0:
    return units & PatmosGenericItinerariesFU::FU_ALU0;
  case 1:
    return units & PatmosGenericItinerariesFU::FU_ALU1;
  default:
    return false;
  }
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePatmosTargetMC() {
  // Register the MC asm info.
  RegisterMCAsmInfoFn X(getThePatmosTarget(), createPatmosMCAsmInfo);

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(getThePatmosTarget(), 
                                      createPatmosMCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(getThePatmosTarget(),
                                    createPatmosMCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(getThePatmosTarget(),
                                          createPatmosMCSubtargetInfo);

  // Register the MCInstPrinter.
  TargetRegistry::RegisterMCInstPrinter(getThePatmosTarget(),
                                        createPatmosMCInstPrinter);

  // Register the MC code emitter
  TargetRegistry::RegisterMCCodeEmitter(getThePatmosTarget(),
                                        createPatmosMCCodeEmitter);

  // Register the asm backend
  TargetRegistry::RegisterMCAsmBackend(getThePatmosTarget(),
                                       createPatmosAsmBackend);

  // Register the asm target streamer.
  TargetRegistry::RegisterAsmTargetStreamer(getThePatmosTarget(),
                                            createPatmosAsmTargetStreamer);

  // Register the object target streamer.
  TargetRegistry::RegisterObjectTargetStreamer(getThePatmosTarget(),
                                               createPatmosObjectTargetStreamer);

}
