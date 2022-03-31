//===-- PatmosMCTargetDesc.h - Patmos Target Descriptions -------*- C++ -*-===//
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

#ifndef _PATMOS_MCTARGETDESC_H_
#define _PATMOS_MCTARGETDESC_H_

#include "llvm/Support/DataTypes.h"
#include "llvm/MC/MCInstrItineraries.h"

#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class StringRef;
class Target;
class Triple;

MCCodeEmitter *createPatmosMCCodeEmitter(const MCInstrInfo &MCII,
                                         const MCRegisterInfo &MRI,
                                         MCContext &Ctx);
										 
MCAsmBackend *createPatmosAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createPatmosELFObjectWriter(const Triple &TT);

bool canIssueInSlotForUnits(unsigned Slot, llvm::InstrStage::FuncUnits &units);

} // End llvm namespace

// Defines symbolic names for Patmos registers.
// This defines a mapping from register name to register number.
#define GET_REGINFO_ENUM
#include "PatmosGenRegisterInfo.inc"

// Defines symbolic names for the Patmos instructions.
#define GET_INSTRINFO_ENUM
#include "PatmosGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "PatmosGenSubtargetInfo.inc"

#endif // _PATMOS_MCTARGETDESC_H_
