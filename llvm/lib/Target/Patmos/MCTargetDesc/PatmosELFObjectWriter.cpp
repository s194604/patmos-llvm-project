//===-- PatmosELFObjectWriter.cpp - Patmos ELF Writer -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/PatmosBaseInfo.h"
#include "MCTargetDesc/PatmosFixupKinds.h"
#include "MCTargetDesc/PatmosMCTargetDesc.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include <list>

using namespace llvm;
using namespace Patmos;

namespace {

  class PatmosELFObjectWriter : public MCELFObjectTargetWriter {
  public:
    PatmosELFObjectWriter(uint8_t OSABI);

    ~PatmosELFObjectWriter() override = default;

    unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                          const MCFixup &Fixup, bool IsPCRel) const override;

    bool needsRelocateWithSymbol(const MCSymbol &Sym,
                                 unsigned Type) const override
    {
      switch (Type) {
      case ELF::R_PATMOS_ALUL_ABS:
      case ELF::R_PATMOS_CFLI_ABS:
      case ELF::R_PATMOS_CFLI_PCREL:
        return true;
      default:
        return false;
      }
    }
  };
}

PatmosELFObjectWriter::PatmosELFObjectWriter(uint8_t OSABI)
  : MCELFObjectTargetWriter(false, OSABI, ELF::EM_PATMOS,
                            /*HasRelocationAddend*/ false) {}

unsigned PatmosELFObjectWriter::getRelocType(MCContext &Ctx,
                                             const MCValue &Target,
                                             const MCFixup &Fixup,
                                             bool IsPCRel) const {
  // TODO determine the type of the relocation, use Patmos types
  switch ((unsigned)Fixup.getKind()) {
  case FK_Data_4:
    return ELF::R_PATMOS_ABS_32;
  case FK_Patmos_BO_7:
    return ELF::R_PATMOS_MEMB_ABS;
  case FK_Patmos_HO_7:
    return ELF::R_PATMOS_MEMH_ABS;
  case FK_Patmos_WO_7:
    return ELF::R_PATMOS_MEMW_ABS;
  case FK_Patmos_abs_ALUi:
    return ELF::R_PATMOS_ALUI_ABS;
  case FK_Patmos_abs_CFLi:
    return ELF::R_PATMOS_CFLI_ABS;
  case FK_Patmos_abs_ALUl:
    return ELF::R_PATMOS_ALUL_ABS;
  case FK_Patmos_stc:
    // TODO do not emit STC format relocations?
    return ELF::R_PATMOS_CFLI_ABS;
  case FK_Patmos_PCrel:
    return ELF::R_PATMOS_CFLI_PCREL;
  default:
    llvm_unreachable("invalid fixup kind!");
  }
}



std::unique_ptr<MCObjectTargetWriter>
llvm::createPatmosELFObjectWriter(const Triple &TT) {
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  MCELFObjectTargetWriter *MOTW = new PatmosELFObjectWriter(OSABI);
  return std::make_unique<PatmosELFObjectWriter>(OSABI);
}
