//==-- PatmosISelLowering.h - Patmos DAG Lowering Interface ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Patmos uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef _LLVM_TARGET_PATMOS_ISELLOWERING_H_
#define _LLVM_TARGET_PATMOS_ISELLOWERING_H_

#include "Patmos.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {
  namespace PatmosISD {
    enum {
      FIRST_NUMBER = ISD::BUILTIN_OP_END,

      /// Return with a flag operand. Operand 0 is the chain operand.
      RET_FLAG,

      /// multiplication
      MUL, MULU,

      /// CALL - These operations represent an abstract call
      /// instruction, which includes a bunch of information.
      CALL = ISD::FIRST_TARGET_MEMORY_OPCODE
    };
  } // end namespace PatmosISD

  class PatmosSubtarget;
  class PatmosTargetMachine;

  class PatmosTargetObjectFile : public TargetLoweringObjectFileELF {
  public:
    void Initialize(MCContext &Ctx, const TargetMachine &TM) {
      TargetLoweringObjectFileELF::Initialize(Ctx, TM);
      InitializeELF(true); // set UseInitArray to true
    }
  };

  class PatmosTargetLowering : public TargetLowering {
  public:
    explicit PatmosTargetLowering(const PatmosTargetMachine &TM,
                                  const PatmosSubtarget &STI);

    /// LowerOperation - Provide custom lowering hooks for some operations.
    SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

    /// getTargetNodeName - This method returns the name of a target specific
    /// DAG node.
    const char *getTargetNodeName(unsigned Opcode) const override;

    EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                           EVT VT) const override;

    unsigned getByValTypeAlignment(Type *Ty,
                                   const DataLayout &DL) const override {
      // Align any type passed by value on the stack to words
      return 4;
    }

    bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
      // Disallow GlobalAddresses to contain offsets (e.g. x + 4)
      // As patmos-ld doesn't know how to fix that when resolving
      // 'x' as a symbol.
      //
      // Setting this to false forces LLVM to instead put the offset
      // in the using instructions, e.g. loads would put '+4' in their
      // immediate offset.
      return false;
    }

    /******************************************************************
     * Jump Tables
     ******************************************************************/

    /// getJumpTableEncoding - Return the entry encoding for a jump table in the
    /// current function.  The returned value is a member of the
    /// MachineJumpTableInfo::JTEntryKind enum.
    unsigned getJumpTableEncoding() const override{
      return MachineJumpTableInfo::EK_Custom32;
    }

    const MCExpr *
    LowerCustomJumpTableEntry(const MachineJumpTableInfo * MJTI,
                              const MachineBasicBlock * MBB, unsigned uid,
                              MCContext &OutContext) const override;

    /******************************************************************
     * Inline asm support
     ******************************************************************/

    ConstraintType getConstraintType(StringRef Constraint) const override;

    std::pair<unsigned, const TargetRegisterClass*>
    getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                 StringRef Constraint, MVT VT) const override;

  private:
    const PatmosSubtarget &Subtarget;

    SDValue LowerCCCCallTo(CallLoweringInfo &CLI,
                           SmallVectorImpl<SDValue> &InVals) const;

    SDValue LowerCCCArguments(SDValue Chain,
                              CallingConv::ID CallConv,
                              bool isVarArg,
                              const SmallVectorImpl<ISD::InputArg> &Ins,
                              SDLoc dl,
                              SelectionDAG &DAG,
                              SmallVectorImpl<SDValue> &InVals) const;

    SDValue LowerCallResult(SDValue Chain, SDValue InFlag,
                            CallingConv::ID CallConv, bool isVarArg,
                            const SmallVectorImpl<ISD::InputArg> &Ins,
                            SDLoc dl, SelectionDAG &DAG,
                            SmallVectorImpl<SDValue> &InVals) const;
  public:
    SDValue LowerCall(CallLoweringInfo &CLI,
                      SmallVectorImpl<SDValue> &InVals) const override;

    SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                                 const SmallVectorImpl<ISD::InputArg> &Ins,
                                 const SDLoc &dl, SelectionDAG &DAG,
                                 SmallVectorImpl<SDValue> &InVals) const override;

    SDValue LowerReturn(SDValue Chain,
                        CallingConv::ID CallConv, bool isVarArg,
                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                        const SmallVectorImpl<SDValue> &OutVals,
                        const SDLoc &dl, SelectionDAG &DAG) const override;

    /// LowerVASTART - Lower the va_start intrinsic to access parameters of
    /// variadic functions.
    SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;

    /// LowerRETURNADDR - Lower the llvm.returnaddress intrinsic.
    SDValue LowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;

    /// LowerFRAMEADDR - Lower the llvm.frameaddress intrinsic.
    SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;

    /// LowerMUL_LOHI - Lower Lo/Hi multiplications.
    SDValue LowerMUL_LOHI(SDValue Op, SelectionDAG &DAG) const;

    /// LowerSTORE - Promote i1 store operations to i8.
    SDValue LowerSTORE(SDValue Op, SelectionDAG &DAG) const;

    /// LowerLOAD - Promote i1 load operations to i8.
    SDValue LowerLOAD(SDValue Op, SelectionDAG &DAG) const;
  };
} // namespace llvm

#endif // _LLVM_TARGET_PATMOS_ISELLOWERING_H_







