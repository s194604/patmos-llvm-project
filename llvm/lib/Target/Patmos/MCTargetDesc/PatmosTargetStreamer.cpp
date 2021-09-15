//===-- PatmosTargetStreamer.cpp - Patmos Target Streamer Methods ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides Patmos specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "PatmosTargetStreamer.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

// pin vtable to this file
void PatmosTargetStreamer::anchor() {}

PatmosTargetStreamer::PatmosTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

PatmosTargetAsmStreamer::PatmosTargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS)
    : PatmosTargetStreamer(S), OS(OS) {}

void PatmosTargetAsmStreamer::EmitFStart(const MCSymbol *Start, 
                               const MCExpr* Size, Align Alignment)
{
  OS << "\t.fstart\t" << *Start << ", " << *Size << ", " << Alignment.value() << "\n";
}

PatmosTargetELFStreamer::PatmosTargetELFStreamer(MCStreamer &S)
    : PatmosTargetStreamer(S) {}

void PatmosTargetELFStreamer::EmitFStart(const MCSymbol *Start, 
	      const MCExpr* Size, Align Alignment)
{
  getStreamer().emitCodeAlignment(Alignment.value());
  getStreamer().emitValue(Size, 4);

}

