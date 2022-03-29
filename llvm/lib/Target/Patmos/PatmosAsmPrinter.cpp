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

#include "PatmosAsmPrinter.h"
#include "PatmosMCInstLower.h"
#include "PatmosMachineFunctionInfo.h"
#include "PatmosTargetMachine.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "InstPrinter/PatmosInstPrinter.h"
#include "MCTargetDesc/PatmosTargetStreamer.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

/// EnableBasicBlockSymbols - If enabled, symbols for basic blocks are emitted
/// containing the name of their IR representation, appended by their
/// MBB number (for uniqueness). Note that this also includes MBBs which are
/// not necessarily branch/call targets.
static cl::opt<bool> EnableBasicBlockSymbols(
  "mpatmos-enable-bb-symbols",
  cl::init(false),
  cl::desc("Enable (additional) generation of symbols "
           "named like the IR basic blocks."),
  cl::Hidden);



void PatmosAsmPrinter::emitFunctionEntryLabel() {
  // Create a temp label that will be emitted at the end of the first cache block (at the end of the function
  // if the function has only one cache block)
  CurrCodeEnd = OutContext.createTempSymbol();

  // emit a function/subfunction start directive
  EmitFStart(CurrentFnSymForSize, CurrCodeEnd, FStartAlignment);

  // Now emit the normal function label
  AsmPrinter::emitFunctionEntryLabel();
}

void PatmosAsmPrinter::emitBasicBlockStart(const MachineBasicBlock &MBB) {

  // First align the block if needed (don't align the first block).
  // We must align first to ensure that any added nops are put
  // before the .fstart, such that when object code is emitted
  // the .size is put right before the label of the block with no nops
  // between.
  auto align = MBB.getAlignment();
  if (align.value() && (MBB.getParent()->getBlockNumbered(0) != &MBB)) {
      emitAlignment(align);
  }

  // Then emit .fstart/.size if needed
  if (isFStart(&MBB) && (MBB.getParent()->getBlockNumbered(0) != &MBB)) {
    // We need an address symbol from the next block
    assert(!MBB.pred_empty() && "Basic block without predecessors do not emit labels, unsupported.");

    MCSymbol *SymStart = MBB.getSymbol();

    // create new end symbol
    CurrCodeEnd = OutContext.createTempSymbol();

    // mark subfunction labels as function labels
    OutStreamer->emitSymbolAttribute(SymStart, MCSA_ELF_TypeFunction);

    // emit a .size directive
    emitDotSize(SymStart, CurrCodeEnd);

    // emit a function/subfunction start directive
    EmitFStart(SymStart, CurrCodeEnd, FStartAlignment);
  }

  // We remove any alignment assigned to the block, to ensure
  // AsmPrinter::EmitBasicBlockStart doesn't also try to align the block
  ((MachineBasicBlock &) MBB).setAlignment(Align(1));
  AsmPrinter::emitBasicBlockStart(MBB);
  ((MachineBasicBlock &) MBB).setAlignment(align);

  emitBasicBlockBegin(MBB);
}

void PatmosAsmPrinter::emitBasicBlockBegin(const MachineBasicBlock &MBB) {
  // If special generation of BB symbols is enabled,
  // do so for every MBB.
  if (EnableBasicBlockSymbols) {
    // create a symbol with the BBs name
    SmallString<128> bbname;
    getMBBIRName(&MBB, bbname);
    MCSymbol *bbsym = OutContext.getOrCreateSymbol(bbname.str());
    OutStreamer->emitLabel(bbsym);

    // set basic block size
    unsigned int bbsize = 0;
    for(MachineBasicBlock::const_instr_iterator i(MBB.instr_begin()),
        ie(MBB.instr_end()); i!=ie; ++i)
    {
      bbsize += i->getDesc().Size;
    }
    OutStreamer->emitELFSize(bbsym, MCConstantExpr::create(bbsize, OutContext));
  }

  // Print loop bound information if needed
  auto loop_bounds = getLoopBounds(&MBB);
  if (loop_bounds.first > -1 || loop_bounds.second > -1){
    OutStreamer->GetCommentOS() << "Loop bound: [";
    if (loop_bounds.first > -1){
      OutStreamer->GetCommentOS() << loop_bounds.first;
    } else {
      OutStreamer->GetCommentOS() << "-";
    }
    OutStreamer->GetCommentOS() << ", ";
    if (loop_bounds.second > -1){
      OutStreamer->GetCommentOS() << loop_bounds.second;
    } else {
      OutStreamer->GetCommentOS() << "-";
    }
    OutStreamer->GetCommentOS() << "]\n";
    OutStreamer->AddBlankLine();
  }

}


void PatmosAsmPrinter::emitBasicBlockEnd(const MachineBasicBlock &MBB) {

  // EmitBasicBlockBegin emits after the label, too late for emitting .fstart,
  // so we do it at the end of the previous block of a cache block start MBB.
  if (&MBB.getParent()->back() == &MBB) return;
  const MachineBasicBlock *Next = MBB.getNextNode();

  bool followedByFStart = isFStart(Next);

  if (followedByFStart) {
    // Next is the start of a new cache block, close the old one before the
    // alignment of the next block
    OutStreamer->emitLabel(CurrCodeEnd);
  }
}

void PatmosAsmPrinter::emitFunctionBodyEnd() {
  // Emit the end symbol of the last cache block
  OutStreamer->emitLabel(CurrCodeEnd);
}

void PatmosAsmPrinter::emitDotSize(MCSymbol *SymStart, MCSymbol *SymEnd) {
  const MCExpr *SizeExpr =
    MCBinaryExpr::createSub(MCSymbolRefExpr::create(SymEnd,   OutContext),
                            MCSymbolRefExpr::create(SymStart, OutContext),
                            OutContext);

  OutStreamer->emitELFSize(SymStart, SizeExpr);
}

void PatmosAsmPrinter::EmitFStart(MCSymbol *SymStart, MCSymbol *SymEnd,
                                     Align Alignment) {
  // emit .fstart SymStart, SymEnd-SymStart
  const MCExpr *SizeExpr =
    MCBinaryExpr::createSub(MCSymbolRefExpr::create(SymEnd,   OutContext),
                            MCSymbolRefExpr::create(SymStart, OutContext),
                            OutContext);

  PatmosTargetStreamer *PTS =
            static_cast<PatmosTargetStreamer*>(OutStreamer->getTargetStreamer());

  PTS->EmitFStart(SymStart, SizeExpr, Alignment);
}

bool PatmosAsmPrinter::isFStart(const MachineBasicBlock *MBB) const {
  // query the machineinfo object - the PatmosFunctionSplitter, or some other
  // pass, has marked all entry blocks already.
  const PatmosMachineFunctionInfo *PMFI =
                                       MF->getInfo<PatmosMachineFunctionInfo>();
  return PMFI->isMethodCacheRegionEntry(MBB);
}


void PatmosAsmPrinter::emitInstruction(const MachineInstr *MI) {
  SmallVector<const MachineInstr*, 2> BundleMIs;
  unsigned Size = 1;

  // Unpack BUNDLE instructions
  if (MI->isBundle()) {
    const MachineBasicBlock *MBB = MI->getParent();
    auto MII = MI->getIterator();

    // First, skip the bundle header instruction,
    // since it is not an real instruction, but is only there to designate the start of a bundle
    ++MII;
    while (MII != MBB->end() && MII->isInsideBundle()) {
      const MachineInstr *MInst = &(*MII);
      assert(!MInst->isPseudo() && "Pseudo instructions must not be bundled!");
      BundleMIs.push_back(MInst);
      ++MII;
    }
    Size = BundleMIs.size();
    assert(Size == MI->getBundleSize() && "Corrupt Bundle!");
  }
  else {
    BundleMIs.push_back(MI);
  }

  // Emit all instructions in the bundle.
  for (unsigned Index = 0; Index < Size; Index++) {
    MCInst MCI;
    MCInstLowering.Lower(BundleMIs[Index], MCI);

    // Set bundle marker
    bool isBundled = (Index < Size - 1);
    MCI.addOperand(MCOperand::createImm(isBundled));

    OutStreamer->emitInstruction(MCI, *TM.getMCSubtargetInfo());
  }
}


bool PatmosAsmPrinter::
isBlockOnlyReachableByFallthrough(const MachineBasicBlock *MBB) const {
  // If this is a landing pad, it isn't a fall through.  If it has no preds,
  // then nothing falls through to it.
  if (MBB->isEHPad() || MBB->pred_empty() || MBB->hasAddressTaken())
    return false;

  // If there isn't exactly one predecessor, it can't be a fall through.
  MachineBasicBlock::const_pred_iterator PI = MBB->pred_begin(), PI2 = PI;
  if (++PI2 != MBB->pred_end())
    return false;

  // The predecessor has to be immediately before this block.
  const MachineBasicBlock *Pred = *PI;

  if (!Pred->isLayoutSuccessor(MBB))
    return false;

  // if the block starts a new cache block, do not fall through (we need to
  // insert cache stuff, even if we only reach this block from a jump from the
  // previous block, and we need the label).
  if (isFStart(MBB))
    return false;

  // If the block is completely empty, then it definitely does fall through.
  if (Pred->empty())
    return true;


  // Here is the difference to the AsmPrinter method;
  // We do not check properties of all terminator instructions
  // (delay slot instructions do not have to be terminators),
  // but instead check if the *last terminator* is an
  // unconditional branch (no barrier)
  MachineBasicBlock::const_iterator I = Pred->end();
  // find last terminator
  while (I != Pred->begin() && !(--I)->isTerminator()) ;
  return I == Pred->end() || !I->isBarrier();
}


bool PatmosAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &OS)

{
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0])
    return true; // Unknown modifier.

  // Print operand for inline-assembler. Basically the same code as in
  // PatmosInstPrinter::printOperand, but for MachineOperand and for
  // inline-assembly. No need for pretty formatting of default ops, output is
  // for AsmParser only.

  // TODO any special handling of predicates (flags) or anything?

  MCInst MCI;
  MCInstLowering.Lower(MI, MCI);

  PatmosInstPrinter PIP(*OutContext.getAsmInfo(), *TM.getMCInstrInfo(),
                        *TM.getMCRegisterInfo());

  PIP.printOperand(&MCI, OpNo, OS);

  return false;
}

bool PatmosAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                                             const char *ExtraCode, raw_ostream &OS)
{
  if (ExtraCode && ExtraCode[0])
     return true; // Unknown modifier.

  const MachineOperand &MO = MI->getOperand(OpNo);
  assert(MO.isReg() && "unexpected inline asm memory operand");
  OS << "[$" << PatmosInstPrinter::getRegisterName(MO.getReg()) << "]";
  return false;
}

/// This function has been copied verbatim from lib/CodeGen/AsmPrinterAsmPrinterInlineAsm.cpp
/// for our own use.
static void EmitGCCInlineAsmStr(const char *AsmStr, const MachineInstr *MI,
                                MachineModuleInfo *MMI, int AsmPrinterVariant,
                                AsmPrinter *AP, unsigned LocCookie,
                                raw_ostream &OS)
{
  int CurVariant = -1;            // The number of the {.|.|.} region we are in.
  const char *LastEmitted = AsmStr; // One past the last character emitted.
  unsigned NumOperands = MI->getNumOperands();

  OS << '\t';

  while (*LastEmitted) {
    switch (*LastEmitted) {
    default: {
      // Not a special case, emit the string section literally.
      const char *LiteralEnd = LastEmitted+1;
      while (*LiteralEnd && *LiteralEnd != '{' && *LiteralEnd != '|' &&
             *LiteralEnd != '}' && *LiteralEnd != '$' && *LiteralEnd != '\n')
        ++LiteralEnd;
      if (CurVariant == -1 || CurVariant == AsmPrinterVariant)
        OS.write(LastEmitted, LiteralEnd-LastEmitted);
      LastEmitted = LiteralEnd;
      break;
    }
    case '\n':
      ++LastEmitted;   // Consume newline character.
      OS << '\n';      // Indent code with newline.
      break;
    case '$': {
      ++LastEmitted;   // Consume '$' character.
      bool Done = true;

      // Handle escapes.
      switch (*LastEmitted) {
      default: Done = false; break;
      case '$':     // $$ -> $
        if (CurVariant == -1 || CurVariant == AsmPrinterVariant)
          OS << '$';
        ++LastEmitted;  // Consume second '$' character.
        break;
      case '(':             // $( -> same as GCC's { character.
        ++LastEmitted;      // Consume '(' character.
        if (CurVariant != -1)
          report_fatal_error("Nested variants found in inline asm string: '" +
                             Twine(AsmStr) + "'");
        CurVariant = 0;     // We're in the first variant now.
        break;
      case '|':
        ++LastEmitted;  // consume '|' character.
        if (CurVariant == -1)
          OS << '|';       // this is gcc's behavior for | outside a variant
        else
          ++CurVariant;   // We're in the next variant.
        break;
      case ')':         // $) -> same as GCC's } char.
        ++LastEmitted;  // consume ')' character.
        if (CurVariant == -1)
          OS << '}';     // this is gcc's behavior for } outside a variant
        else
          CurVariant = -1;
        break;
      }
      if (Done) break;

      bool HasCurlyBraces = false;
      if (*LastEmitted == '{') {     // ${variable}
        ++LastEmitted;               // Consume '{' character.
        HasCurlyBraces = true;
      }

      // If we have ${:foo}, then this is not a real operand reference, it is a
      // "magic" string reference, just like in .td files.  Arrange to call
      // PrintSpecial.
      if (HasCurlyBraces && *LastEmitted == ':') {
        ++LastEmitted;
        const char *StrStart = LastEmitted;
        const char *StrEnd = strchr(StrStart, '}');
        if (!StrEnd)
          report_fatal_error("Unterminated ${:foo} operand in inline asm"
                             " string: '" + Twine(AsmStr) + "'");

        std::string Val(StrStart, StrEnd);
        AP->PrintSpecial(MI, OS, Val.c_str());
        LastEmitted = StrEnd+1;
        break;
      }

      const char *IDStart = LastEmitted;
      const char *IDEnd = IDStart;
      while (*IDEnd >= '0' && *IDEnd <= '9') ++IDEnd;

      unsigned Val;
      if (StringRef(IDStart, IDEnd-IDStart).getAsInteger(10, Val))
        report_fatal_error("Bad $ operand number in inline asm string: '" +
                           Twine(AsmStr) + "'");
      LastEmitted = IDEnd;

      char Modifier[2] = { 0, 0 };

      if (HasCurlyBraces) {
        // If we have curly braces, check for a modifier character.  This
        // supports syntax like ${0:u}, which correspond to "%u0" in GCC asm.
        if (*LastEmitted == ':') {
          ++LastEmitted;    // Consume ':' character.
          if (*LastEmitted == 0)
            report_fatal_error("Bad ${:} expression in inline asm string: '" +
                               Twine(AsmStr) + "'");

          Modifier[0] = *LastEmitted;
          ++LastEmitted;    // Consume modifier character.
        }

        if (*LastEmitted != '}')
          report_fatal_error("Bad ${} expression in inline asm string: '" +
                             Twine(AsmStr) + "'");
        ++LastEmitted;    // Consume '}' character.
      }

      if (Val >= NumOperands-1)
        report_fatal_error("Invalid $ operand number in inline asm string: '" +
                           Twine(AsmStr) + "'");

      // Okay, we finally have a value number.  Ask the target to print this
      // operand!
      if (CurVariant == -1 || CurVariant == AsmPrinterVariant) {
        unsigned OpNo = InlineAsm::MIOp_FirstOperand;

        bool Error = false;

        // Scan to find the machine operand number for the operand.
        for (; Val; --Val) {
          if (OpNo >= MI->getNumOperands()) break;
          unsigned OpFlags = MI->getOperand(OpNo).getImm();
          OpNo += InlineAsm::getNumOperandRegisters(OpFlags) + 1;
        }

        // We may have a location metadata attached to the end of the
        // instruction, and at no point should see metadata at any
        // other point while processing. It's an error if so.
        if (OpNo >= MI->getNumOperands() ||
            MI->getOperand(OpNo).isMetadata()) {
          Error = true;
        } else {
          unsigned OpFlags = MI->getOperand(OpNo).getImm();
          ++OpNo;  // Skip over the ID number.

          // FIXME: Shouldn't arch-independent output template handling go into
          // PrintAsmOperand?
          // Labels are target independent.
          if (MI->getOperand(OpNo).isBlockAddress()) {
            const BlockAddress *BA = MI->getOperand(OpNo).getBlockAddress();
            MCSymbol *Sym = AP->GetBlockAddressSymbol(BA);
            Sym->print(OS, AP->MAI);
            MMI->getContext().registerInlineAsmLabel(Sym);
          } else if (MI->getOperand(OpNo).isMBB()) {
            const MCSymbol *Sym = MI->getOperand(OpNo).getMBB()->getSymbol();
            Sym->print(OS, AP->MAI);
          } else if (Modifier[0] == 'l') {
            Error = true;
          } else if (InlineAsm::isMemKind(OpFlags)) {
            Error = AP->PrintAsmMemoryOperand(
                MI, OpNo, Modifier[0] ? Modifier : nullptr, OS);
          } else {
            Error = AP->PrintAsmOperand(MI, OpNo,
                                        Modifier[0] ? Modifier : nullptr, OS);
          }
        }
        if (Error) {
          std::string msg;
          raw_string_ostream Msg(msg);
          Msg << "invalid operand in inline asm: '" << AsmStr << "'";
          MMI->getModule()->getContext().emitError(LocCookie, Msg.str());
        }
      }
      break;
    }
    }
  }
  OS << '\n' << (char)0;  // null terminate string.
}

/// This method has been copied from lib/CodeGen/AsmPrinterAsmPrinterInlineAsm.cpp
/// since its 'emitInlineAsm" method is private, but we need it to get the
/// size of instructions in inline assembly.
/// We copied the code needed to make the size work.
void PatmosAsmPrinter::mockEmitInlineAsmForSizeCount(const MachineInstr *MI) const {
  assert(MI->isInlineAsm() && "printInlineAsm only works on inline asms");

  // Count the number of register definitions to find the asm string.
  unsigned NumDefs = 0;
  for (; MI->getOperand(NumDefs).isReg() && MI->getOperand(NumDefs).isDef();
      ++NumDefs)
   assert(NumDefs != MI->getNumOperands()-2 && "No asm string?");

  assert(MI->getOperand(NumDefs).isSymbol() && "No asm string?");

  // Disassemble the AsmStr, printing out the literal pieces, the operands, etc.
  const char *AsmStr = MI->getOperand(NumDefs).getSymbolName();

  // If this asmstr is empty, do nothing
  if (AsmStr[0] == 0) {
   return;
  }

  // Emit the inline asm to a temporary string so we can emit it through
  // EmitInlineAsm.
  SmallString<256> StringData;
  raw_svector_ostream OS(StringData);
  AsmPrinter *AP = (AsmPrinter*)this;
  EmitGCCInlineAsmStr(AsmStr, MI, MMI, (int)MAI->getAssemblerDialect(), AP, (unsigned)0, OS);

  std::unique_ptr<MemoryBuffer> Buffer;
  // The inline asm source manager will outlive AsmStr, so make a copy of the
  // string for SourceMgr to own.
  Buffer = MemoryBuffer::getMemBufferCopy(OS.str(), "<inline asm>");
  auto srcMgr = SourceMgr();
  unsigned BufNum = srcMgr.AddNewSourceBuffer(std::move(Buffer), SMLoc());

  std::unique_ptr<MCAsmParser> Parser(createMCAsmParser(
      srcMgr, OutContext, *OutStreamer, *MAI, BufNum));
  std::unique_ptr<MCTargetAsmParser> TAP(TM.getTarget().createMCAsmParser(
      *TM.getMCSubtargetInfo(), *Parser, *TM.getMCInstrInfo(), TM.Options.MCOptions));
  Parser->setTargetParser(*TAP.get());
  int Res = Parser->Run(true,true);

  if (Res)
      report_fatal_error("Error parsing inline asm\n");
}

////////////////////////////////////////////////////////////////////////////////

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePatmosAsmPrinter() {
  RegisterAsmPrinter<PatmosAsmPrinter> X(getThePatmosTarget());
}
