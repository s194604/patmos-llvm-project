//===-- PatmosTargetStreamer.h - Patmos Target Streamer --------*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef PATMOSTARGETSTREAMER_H
#define PATMOSTARGETSTREAMER_H

#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {
class PatmosTargetStreamer : public MCTargetStreamer {
  virtual void anchor();

public:
  PatmosTargetStreamer(MCStreamer &S);

  /// EmitFStart - Emit a function block start block, including the
  ///              function size and alignment
  /// \param Start - The start symbol of the function block, should be emitted
  ///                immediately after this directive.
  /// \param Size - The size of the block in bytes.
  /// \param Alignment - The alignment in bytes, should be a power of 2.
  virtual void EmitFStart(const MCSymbol *Start, const MCExpr* Size,
                          Align Alignment) = 0;
};

// This part is for ascii assembly output
class PatmosTargetAsmStreamer : public PatmosTargetStreamer {
  formatted_raw_ostream &OS;

public:
  PatmosTargetAsmStreamer(MCStreamer &S,formatted_raw_ostream &OS);

  void EmitFStart(const MCSymbol *Start, const MCExpr* Size,
                  Align Alignment) override;
};

// This part is for ELF object output
class PatmosTargetELFStreamer : public PatmosTargetStreamer {
public:
  PatmosTargetELFStreamer(MCStreamer &S);

  void EmitFStart(const MCSymbol *Start, const MCExpr* Size,
                  Align Alignment) override;
};

}

#endif
