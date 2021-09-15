//===-- PatmosAsmBackend.cpp - Patmos Asm Backend  ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the PatmosAsmBackend class.
//
//===----------------------------------------------------------------------===//
//

#include "MCTargetDesc/PatmosAsmBackend.h"
#include "MCTargetDesc/PatmosFixupKinds.h"
#include "MCTargetDesc/PatmosMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace Patmos;

// Prepare value for the target space for it
static unsigned adjustFixupValue(const MCFixup &Fixup, uint64_t Value) {

  unsigned Kind = Fixup.getKind();

  // Add/subtract and shift
  switch (Kind) {
  // TODO check: do we need to shift the load/store offsets here or is this done
  // earlier in the compiler?
  case FK_Patmos_HO_7:
    Value >>= 1;
    break;
  case FK_Patmos_WO_7:
  case FK_Patmos_abs_CFLi:
  case FK_Patmos_PCrel:
    Value >>= 2;
    break;
  }

  return Value;
}

std::unique_ptr<MCObjectTargetWriter>
PatmosAsmBackend::createObjectTargetWriter() const {
  return createPatmosELFObjectWriter(TheTriple);
}

/// ApplyFixup - Apply the \p Value for given \p Fixup into the provided
/// data fragment, at the offset specified by the fixup and following the
/// fixup kind as appropriate.
void PatmosAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                const MCValue &Target,
                                MutableArrayRef<char> Data, uint64_t Value,
                                bool IsResolved,
                                const MCSubtargetInfo *STI) const {
  MCFixupKind Kind = Fixup.getKind();
  Value = adjustFixupValue(Fixup, Value);

  if (!Value)
    return; // Doesn't change encoding.

    unsigned TargetSize = getFixupKindInfo(Kind).TargetSize;
    unsigned TargetOffset = getFixupKindInfo(Kind).TargetOffset;

    // Where do we start in the object
    unsigned Offset = Fixup.getOffset();
    // Number of bytes we need to fixup
    unsigned NumBytes = (TargetSize + 7) / 8;
    // Used to point to big endian bytes
    unsigned FullSize = (TargetOffset + TargetSize + 7) / 8;

    // Grab current value, if any, from bits.
    uint64_t CurVal = 0;

    for (unsigned i = 0; i != NumBytes; ++i) {
      unsigned Idx = (FullSize - 1 - i);
      CurVal |= (uint64_t)((uint8_t)Data[Offset + Idx]) << (i*8);
    }

    uint64_t Mask = ((uint64_t)(-1) >> (64 - TargetSize));
    unsigned Shift = FullSize * 8 - (TargetOffset + TargetSize);

    CurVal |= (Value & Mask) << Shift;
    // Write out the fixed up bytes back to the code/data bits.
    for (unsigned i = 0; i != NumBytes; ++i) {
      unsigned Idx = (FullSize - 1 - i);
      Data[Offset + Idx] = (uint8_t)((CurVal >> (i*8)) & 0xff);
    }
}

const MCFixupKindInfo &PatmosAsmBackend::
getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[Patmos::NumTargetFixupKinds] = {
    // This table *must* be in same the order of FK_* kinds in
    // PatmosFixupKinds.h.
    //
    // name                    offset  bits  flags
    { "FK_Patmos_BO_7" ,       25,      7,   0 }, // 0 bit shifted, unsigned (byte aligned)
    { "FK_Patmos_SO_7" ,       25,      7,   0 }, // 1 bit shifted, unsigned (half-word aligned)
    { "FK_Patmos_WO_7" ,       25,      7,   0 }, // 2 bit shifted, unsigned (word aligned)
    { "FK_Patmos_abs_ALUi",    20,     12,   0 }, // ALU immediate, unsigned
    { "FK_Patmos_abs_CFLi",    10,     22,   0 }, // 2 bit shifted, unsigned, for call
    { "FK_Patmos_abs_ALUl",    32,     32,   0 }, // ALU immediate, unsigned
    { "FK_Patmos_stc",         14,     18,   0 }, // 2 bit shifted, unsigned, for stack control
    { "FK_Patmos_PCrel",       10,     22,   MCFixupKindInfo::FKF_IsPCRel }, // 2 bit shifted, signed, PC relative
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

/// WriteNopData - Write an (optimal) nop sequence of Count bytes
/// to the given output. If the target cannot generate such a sequence,
/// it should return an error.
///
/// \return - True on success.
bool PatmosAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count) const {
  // Count is in terms of of ValueSize, which is always 1 Byte for ELF.
  // This method is used to create a NOP slide for code segment alignment  
  // OW handles byteorder stuff.

  if ((Count % 4) != 0)
    return false;

  // We should somehow initialize the NOP instruction code from TableGen, but
  // I do not see how (without creating a new CodeEmitter and everything from
  // Target)

  for (uint64_t i = 0; i < Count; i += 4)
    // "(p0) sub r0 = r0, 0"
    support::endian::write<uint32_t>(OS, 0x00400000, Endian);

  return true;
}

MCAsmBackend *llvm::createPatmosAsmBackend(const Target &T,
                                           const MCSubtargetInfo &STI,
                                           const MCRegisterInfo &MRI,
                                           const MCTargetOptions &Options) {
  return new PatmosAsmBackend(T, MRI, STI.getTargetTriple(), STI.getCPU());
}
