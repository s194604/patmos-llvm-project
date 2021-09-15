//===-- PatmosAsmPrinter.cpp - Patmos LLVM assembly writer ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is here from the legacy asm printing infrastructure and
// probably will vanish one day.
//
//===----------------------------------------------------------------------===//

#ifndef _LLVM_TARGET_PATMOS_ASMPRINTER_H_
#define _LLVM_TARGET_PATMOS_ASMPRINTER_H_

#include "PatmosTargetMachine.h"
#include "PatmosMCInstLower.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCContext.h"

namespace llvm {
  class PatmosAsmPrinter : public AsmPrinter {
  private:
    PatmosTargetMachine *PTM;

    PatmosMCInstLower MCInstLowering;

    /// Alignment to use for FStart directives, in log2(bytes).
    Align FStartAlignment;

    // symbol to use for the end of the currently emitted subfunction
    MCSymbol *CurrCodeEnd;

  public:
    PatmosAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)), MCInstLowering(OutContext, *this), CurrCodeEnd(0)
    {
      if (!(PTM = static_cast<PatmosTargetMachine*>(&TM))) {
        llvm_unreachable("PatmosAsmPrinter must be initialized with a Patmos target configuration.");
      }
      PTM->Options.MCOptions.MCSaveTempLabels = true;

      FStartAlignment = PTM->getSubtargetImpl()->getMinSubfunctionAlignment();
    }

    StringRef getPassName() const override {
      return "Patmos Assembly Printer";
    }

    void setMachineModuleInfo(MachineModuleInfo* mmi) {
      MMI = mmi;
    }

    void emitFunctionEntryLabel() override;

    void emitBasicBlockStart(const MachineBasicBlock &MBB) override;
    void emitBasicBlockBegin(const MachineBasicBlock &MBB);
    void emitBasicBlockEnd(const MachineBasicBlock &) override;

    void emitFunctionBodyEnd() override;

    // called in the framework for instruction printing
    void emitInstruction(const MachineInstr *MI) override;

    /// EmitDotSize - Emit a .size directive using SymEnd - SymStart.
    void emitDotSize(MCSymbol *SymStart, MCSymbol *SymEnd);

    /// isBlockOnlyReachableByFallthough - Return true if the basic block has
    /// exactly one predecessor and the control transfer mechanism between
    /// the predecessor and this block is a fall-through.
    ///
    /// This overrides AsmPrinter's implementation to handle delay slots.
    bool isBlockOnlyReachableByFallthrough(const MachineBasicBlock *MBB) const override;

    //===------------------------------------------------------------------===//
    // Inline Asm Support
    //===------------------------------------------------------------------===//

    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                         const char *ExtraCode, raw_ostream &OS) override;

    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                               const char *ExtraCode, raw_ostream &OS) override;

    void mockEmitInlineAsmForSizeCount(const MachineInstr *MI) const;
  private:
    /// mark the start of an subfunction relocation area.
    void EmitFStart(MCSymbol *SymStart, MCSymbol *SymEnd,
                    Align Alignment);

    bool isFStart(const MachineBasicBlock *MBB) const;
  };

} // end of llvm namespace

#endif
