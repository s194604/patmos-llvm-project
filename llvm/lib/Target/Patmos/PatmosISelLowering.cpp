//===-- PatmosISelLowering.cpp - Patmos DAG Lowering Implementation  ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the PatmosTargetLowering class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "patmos-lower"

#include "PatmosISelLowering.h"
#include "Patmos.h"
#include "PatmosMachineFunctionInfo.h"
#include "PatmosTargetMachine.h"
#include "PatmosSubtarget.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/MC/MCExpr.h"
using namespace llvm;


PatmosTargetLowering::PatmosTargetLowering(const PatmosTargetMachine &tm,
                                           const PatmosSubtarget &STI) :
  TargetLowering(tm), Subtarget(STI) {

  // Set up the register classes.
  // SRegs are not used for computations.
  addRegisterClass(MVT::i32, &Patmos::RRegsRegClass);
  addRegisterClass(MVT::i1,  &Patmos::PRegsRegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(STI.getRegisterInfo());

  // Provide all sorts of operation actions

  // Jump is Expensive. Don't create extra control flow for 'and', 'or'
  // condition branches.
  setJumpIsExpensive(true);

  setStackPointerRegisterToSaveRestore(Patmos::RSP);
  setBooleanContents(ZeroOrOneBooleanContent);

  // Allow rather aggressive inlining of memcpy and friends
  MaxStoresPerMemset = 32;
  MaxStoresPerMemsetOptSize = 8;
  MaxStoresPerMemcpy = 16;
  MaxStoresPerMemcpyOptSize = 4;
  MaxStoresPerMemmove = 16;
  MaxStoresPerMemmoveOptSize = 4;

  // We require word alignment at least (in log2 bytes here), if code requires 
  // an other alignment, e.g., due to the method-cache, it will be handled 
  // later.
  setMinFunctionAlignment(Align(2));
  setPrefFunctionAlignment(Subtarget.getMinSubfunctionAlignment());

  // Enable using divmod functions
  setLibcallName(RTLIB::SDIVREM_I32, "__divmodsi4");
  setLibcallName(RTLIB::UDIVREM_I32, "__udivmodsi4");
  setLibcallName(RTLIB::SDIVREM_I64, "__divmoddi4");
  setLibcallName(RTLIB::UDIVREM_I64, "__udivmoddi4");

  setOperationAction(ISD::LOAD,   MVT::i1, Custom);
  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1, Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1, Promote);
  }
  setOperationAction(ISD::STORE, MVT::i1, Custom);

  setOperationAction(ISD::SIGN_EXTEND, MVT::i1, Promote);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i1, Promote);
  setOperationAction(ISD::ANY_EXTEND,  MVT::i1, Promote);
  // NB: Several operations simply do not get promoted, e.g.,
  //     arithmetic operations like add, sub, ...
  //     We try to solve them by isel patterns, e.g. add i1 -> xor i1

  // Expand to S/UMUL_LOHI
  setOperationAction(ISD::MULHS, MVT::i32, Expand);
  setOperationAction(ISD::MULHU, MVT::i32, Expand);
  setOperationAction(ISD::SMUL_LOHI, MVT::i32, Custom);
  setOperationAction(ISD::UMUL_LOHI, MVT::i32, Custom);
  // Patmos has no DIV, REM or DIVREM operations.
  setOperationAction(ISD::SDIV, MVT::i32, Expand);
  setOperationAction(ISD::UDIV, MVT::i32, Expand);
  setOperationAction(ISD::SREM, MVT::i32, Expand);
  setOperationAction(ISD::UREM, MVT::i32, Expand);
  setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
  setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

  // we don't have carry setting add/sub instructions.
  // TODO custom lowering with predicates?
  setOperationAction(ISD::CARRY_FALSE, MVT::i32, Expand);
  setOperationAction(ISD::ADDC, MVT::i32, Expand);
  setOperationAction(ISD::SUBC, MVT::i32, Expand);
  setOperationAction(ISD::ADDE, MVT::i32, Expand);
  setOperationAction(ISD::SUBE, MVT::i32, Expand);
  // add/sub/mul with overflow
  setOperationAction(ISD::SADDO, MVT::i32, Expand);
  setOperationAction(ISD::UADDO, MVT::i32, Expand);
  setOperationAction(ISD::SSUBO, MVT::i32, Expand);
  setOperationAction(ISD::USUBO, MVT::i32, Expand);
  setOperationAction(ISD::SMULO, MVT::i32, Expand);
  setOperationAction(ISD::UMULO, MVT::i32, Expand);

  // no bit-fiddling
  setOperationAction(ISD::BSWAP, MVT::i32, Expand);
  setOperationAction(ISD::CTTZ , MVT::i32, Expand);
  setOperationAction(ISD::CTLZ , MVT::i32, Expand);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF, MVT::i32, Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF, MVT::i32, Expand);
  setOperationAction(ISD::CTPOP, MVT::i32, Expand);

  setOperationAction(ISD::SIGN_EXTEND, MVT::i8,  Expand);
  setOperationAction(ISD::SIGN_EXTEND, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND, MVT::i32, Expand);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i8,  Expand);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i16, Expand);
  setOperationAction(ISD::ZERO_EXTEND, MVT::i32, Expand);
  setOperationAction(ISD::ANY_EXTEND,  MVT::i8, Expand);
  setOperationAction(ISD::ANY_EXTEND,  MVT::i16, Expand);
  setOperationAction(ISD::ANY_EXTEND,  MVT::i32, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8,  Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i32, Expand);

  setOperationAction(ISD::ROTL , MVT::i32, Expand);
  setOperationAction(ISD::ROTR , MVT::i32, Expand);

  setOperationAction(ISD::SHL_PARTS, MVT::i32,   Expand);
  setOperationAction(ISD::SRA_PARTS, MVT::i32,   Expand);
  setOperationAction(ISD::SRL_PARTS, MVT::i32,   Expand);

  setOperationAction(ISD::SELECT_CC, MVT::i1,    Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i8,    Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i16,   Expand);
  setOperationAction(ISD::SELECT_CC, MVT::i32,   Expand);
  setOperationAction(ISD::SELECT_CC, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC,     MVT::i1,    Expand);
  setOperationAction(ISD::BR_CC,     MVT::i8,    Expand);
  setOperationAction(ISD::BR_CC,     MVT::i16,   Expand);
  setOperationAction(ISD::BR_CC,     MVT::i32,   Expand);
  setOperationAction(ISD::BR_CC,     MVT::Other, Expand);

  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32, Expand);

  // handling of variadic parameters
  setOperationAction(ISD::VASTART     , MVT::Other, Custom);
  setOperationAction(ISD::VAARG       , MVT::Other, Expand);
  setOperationAction(ISD::VACOPY      , MVT::Other, Expand);
  setOperationAction(ISD::VAEND       , MVT::Other, Expand);
  // llvm.stacksave and restore, rarely seen
  setOperationAction(ISD::STACKSAVE   , MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  setOperationAction(ISD::PCMARKER,  MVT::Other, Expand);
  // TODO expand floating point stuff?

}

SDValue PatmosTargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {

  switch (Op.getOpcode()) {
    case ISD::LOAD:               return LowerLOAD(Op,DAG);
    case ISD::STORE:              return LowerSTORE(Op,DAG);
    case ISD::SMUL_LOHI:
    case ISD::UMUL_LOHI:          return LowerMUL_LOHI(Op, DAG);
    case ISD::VASTART:            return LowerVASTART(Op, DAG);
    case ISD::FRAMEADDR:          return LowerFRAMEADDR(Op, DAG);
    case ISD::RETURNADDR:         return LowerRETURNADDR(Op, DAG);
    default:
      llvm_unreachable("unimplemented operation");
  }
}

EVT PatmosTargetLowering::getSetCCResultType(const DataLayout &DL,
                                             LLVMContext &Context,
                                             EVT VT) const
{
  // All our compare results should be i1
  return MVT::i1;
}

const MCExpr * PatmosTargetLowering::LowerCustomJumpTableEntry(
                          const MachineJumpTableInfo * MJTI,
                          const MachineBasicBlock * MBB, unsigned uid,
                          MCContext &OutContext) const
{
  // Note: see also PatmosMCInstLower::LowerSymbolOperand
  return MCSymbolRefExpr::create(MBB->getSymbol(), OutContext);
}

//===----------------------------------------------------------------------===//
//                      Custom Lower Operation
//===----------------------------------------------------------------------===//

SDValue PatmosTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  LoadSDNode *load = static_cast<LoadSDNode*>(Op.getNode());

  assert(load->getMemoryVT() == MVT::i1);

  SDValue newLoad = DAG.getLoad(ISD::UNINDEXED, ISD::ZEXTLOAD, MVT::i32,
                                Op, load->getChain(),
                                load->getBasePtr(), load->getOffset(), MVT::i8,
                                load->getMemOperand());

  SDValue newTrunc = DAG.getZExtOrTrunc(newLoad, Op, MVT::i1);

  SDValue Ops[2] = { newTrunc, newLoad.getOperand(0) };
  return DAG.getMergeValues(Ops, Op);
}

SDValue PatmosTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *store = static_cast<StoreSDNode*>(Op.getNode());

  assert(store->getMemoryVT() == MVT::i1);

  SDValue newVal = DAG.getZExtOrTrunc(store->getValue(), Op, MVT::i32);

  return DAG.getTruncStore(store->getChain(), Op, newVal,
                           store->getBasePtr(), MVT::i1,
                           store->getMemOperand());
}

SDValue PatmosTargetLowering::LowerMUL_LOHI(SDValue Op,
                                            SelectionDAG &DAG) const {
  unsigned MultOpc;
  EVT Ty = Op.getValueType();
  SDLoc dl(Op);

  assert(Ty == MVT::i32 && "Unexpected type for MUL");

  MultOpc = (Op.getOpcode()==ISD::UMUL_LOHI)? PatmosISD::MULU
                                            : PatmosISD::MUL;

  SDValue Mul = DAG.getNode(MultOpc, dl, MVT::Glue,
                            Op.getOperand(0), Op.getOperand(1));

  SDValue InChain = DAG.getEntryNode();
  SDValue InGlue = Mul;

  SDValue CopyFromLo = DAG.getCopyFromReg(InChain, dl,
      Patmos::SL, Ty, InGlue);
  DAG.ReplaceAllUsesOfValueWith(Op.getValue(0), CopyFromLo);
  InChain = CopyFromLo.getValue(1);
  InGlue = CopyFromLo.getValue(2);

  SDValue CopyFromHi = DAG.getCopyFromReg(InChain, dl,
      Patmos::SH, Ty, InGlue);
  DAG.ReplaceAllUsesOfValueWith(Op.getValue(1), CopyFromHi);

  SDValue Vals[] = { CopyFromLo, CopyFromHi };
  return DAG.getMergeValues(Vals, dl);
}

SDValue PatmosTargetLowering::LowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const {
  auto MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setReturnAddressIsTaken(true);

  EVT VT = Op.getValueType();
  SDLoc dl(Op);
  unsigned Depth = Op.getConstantOperandVal(0);
  if (Depth) {
    report_fatal_error("Return address can only be determined for the current frame in " +
                       DAG.getMachineFunction().getName());
  }

  // TODO we only return the offset here .. how can we make this both a base and offset??
  SDValue RetAddr = DAG.getCopyFromReg(DAG.getEntryNode(), dl,
                                         Patmos::SRO, VT);
  return RetAddr;
}

SDValue PatmosTargetLowering::LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const {
  auto MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setFrameAddressIsTaken(true);

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  unsigned Depth = Op.getConstantOperandVal(0);
  if (Depth) {
    report_fatal_error("Frame address can only be determined for current frame in " +
                       DAG.getMachineFunction().getName());
  }

  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), DL,
                                         Patmos::RFP, VT);
  return FrameAddr;
}

//===----------------------------------------------------------------------===//
//                      Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "PatmosGenCallingConv.inc"

SDValue
PatmosTargetLowering::LowerFormalArguments(SDValue Chain,
                                           CallingConv::ID CallConv, bool isVarArg,
                                           const SmallVectorImpl<ISD::InputArg> &Ins,
                                           const SDLoc &dl, SelectionDAG &DAG,
                                           SmallVectorImpl<SDValue> &InVals
                                           ) const {

  switch (CallConv) {
  default:
    llvm_unreachable("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
    return LowerCCCArguments(Chain, CallConv, isVarArg, Ins, dl, DAG, InVals);
  }
}

SDValue
PatmosTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                SmallVectorImpl<SDValue> &InVals) const {
  // Patmos target does not yet support tail call optimization.
  CLI.IsTailCall = false;

  switch (CLI.CallConv) {
  default:
    llvm_unreachable("Unsupported calling convention");
  case CallingConv::Fast:
  case CallingConv::C:
    return LowerCCCCallTo(CLI, InVals);
  }
}

/// LowerCCCArguments - transform physical registers into virtual registers and
/// generate load operations for arguments places on the stack.
// FIXME: struct return stuff
// FIXME: varargs
SDValue
PatmosTargetLowering::LowerCCCArguments(SDValue Chain,
                                        CallingConv::ID CallConv,
                                        bool isVarArg,
                                        const SmallVectorImpl<ISD::InputArg>
                                        &Ins,
                                        SDLoc dl,
                                        SelectionDAG &DAG,
                                        SmallVectorImpl<SDValue> &InVals
                                        ) const {

  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  MachineRegisterInfo &RegInfo = DAG.getMachineFunction().getRegInfo();
  PatmosMachineFunctionInfo &PMFI = *DAG.getMachineFunction().getInfo<PatmosMachineFunctionInfo>();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;

  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_Patmos);

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    if (VA.isRegLoc()) {
      // Arguments passed in registers
      EVT RegVT = VA.getLocVT();
      switch (RegVT.getSimpleVT().SimpleTy) {
      default:
        {
          LLVM_DEBUG(dbgs() << "LowerFormalArguments Unhandled argument type: "
               << RegVT.getSimpleVT().SimpleTy << "\n");
          llvm_unreachable(0);
        }
      case MVT::i32:
        unsigned VReg =
          RegInfo.createVirtualRegister(&Patmos::RRegsRegClass);
        RegInfo.addLiveIn(VA.getLocReg(), VReg);
        SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, VReg, RegVT);

        // If this is an 8/16-bit value, it is really passed promoted to 32
        // bits. Insert an assert[sz]ext to capture this, then truncate to the
        // right size.
        if (VA.getLocInfo() == CCValAssign::SExt)
          ArgValue = DAG.getNode(ISD::AssertSext, dl, RegVT, ArgValue,
                                 DAG.getValueType(VA.getValVT()));
        else if (VA.getLocInfo() == CCValAssign::ZExt)
          ArgValue = DAG.getNode(ISD::AssertZext, dl, RegVT, ArgValue,
                                 DAG.getValueType(VA.getValVT()));

        if (VA.getLocInfo() != CCValAssign::Full)
          ArgValue = DAG.getNode(ISD::TRUNCATE, dl, VA.getValVT(), ArgValue);

        InVals.push_back(ArgValue);
      }
    } else {
      // Sanity check
      assert(VA.isMemLoc());
      // Load the argument to a virtual register
      unsigned ObjSize = VA.getLocVT().getSizeInBits()/8;
      // Create the frame index object for this incoming parameter...
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);

      // XXX handle alignment of large arguments
      if (ObjSize > 4 || MFI.getObjectAlignment(FI) > 4) {
        errs() << "LowerFormalArguments Unhandled argument type: "
             << EVT(VA.getLocVT()).getEVTString()
             << "\n";
        report_fatal_error("Stack alignment other than 4 byte is not supported");
      }

      // Create the SelectionDAG nodes corresponding to a load
      //from this parameter
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      InVals.push_back(DAG.getLoad(VA.getLocVT(), dl, Chain, FIN,
                                   MachinePointerInfo::getFixedStack(DAG.getMachineFunction(),FI)));
    }
  }

  // handle parameters of variadic functions.
  if (isVarArg) {
    // create a fixed FI to reference the variadic parameters passed on the 
    // stack and store it with the patmos machine function info.
    PMFI.setVarArgsFI(MFI.CreateFixedObject(4, CCInfo.getNextStackOffset(),
                                             true));
  }

  return Chain;
}

SDValue
PatmosTargetLowering::LowerReturn(SDValue Chain,
                                  CallingConv::ID CallConv, bool isVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &dl, SelectionDAG &DAG) const {

  // CCValAssign - represent the assignment of the return value to a location
  SmallVector<CCValAssign, 16> RVLocs;

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs, *DAG.getContext());

  // Analyze return values.
  CCInfo.AnalyzeReturn(Outs, RetCC_Patmos);

  SDValue Flag;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(),
                             OutVals[i], Flag);

    // Guarantee that all emitted copies are stuck together,
    // avoiding something bad.
    Flag = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  auto Opc = PatmosISD::RET_FLAG;

  RetOps[0] = Chain;  // Update chain.

  if (Flag.getNode())
    RetOps.push_back(Flag);

  // Return
  return DAG.getNode(Opc, dl, MVT::Other, RetOps);
}

/// LowerCCCCallTo - functions arguments are copied from virtual regs to
/// (physical regs)/(stack frame), CALLSEQ_START and CALLSEQ_END are emitted.
/// TODO: sret.
SDValue
PatmosTargetLowering::LowerCCCCallTo(CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const
{
  SelectionDAG &DAG                     = CLI.DAG;
  const SDLoc dl                        = CLI.DL;
  SmallVector<ISD::OutputArg, 32> &Outs = CLI.Outs;
  SmallVector<SDValue, 32> &OutVals     = CLI.OutVals;
  SmallVector<ISD::InputArg, 32> &Ins   = CLI.Ins;
  SDValue Chain                         = CLI.Chain;
  SDValue Callee                        = CLI.Callee;
  CallingConv::ID CallConv              = CLI.CallConv;
  bool isVarArg                         = CLI.IsVarArg;

  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs, *DAG.getContext());

  CCInfo.AnalyzeCallOperands(Outs, CC_Patmos);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();

  Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, dl);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue StackPtr;

  // Walk the register/memloc assignments, inserting copies/loads.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    SDValue Arg = OutVals[i];

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
      default: llvm_unreachable("Unknown loc info!");
      case CCValAssign::Full: break;
      case CCValAssign::SExt:
        Arg = DAG.getNode(ISD::SIGN_EXTEND, dl, VA.getLocVT(), Arg);
        break;
      case CCValAssign::ZExt:
        Arg = DAG.getNode(ISD::ZERO_EXTEND, dl, VA.getLocVT(), Arg);
        break;
      case CCValAssign::AExt:
        Arg = DAG.getNode(ISD::ANY_EXTEND, dl, VA.getLocVT(), Arg);
        break;
    }

    // Arguments that can be passed on register must be kept at RegsToPass
    // vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      assert(VA.isMemLoc());

      if (StackPtr.getNode() == 0)
        StackPtr = DAG.getCopyFromReg(Chain, dl, Patmos::RSP, getPointerTy(DAG.getDataLayout()));

      SDValue PtrOff = DAG.getNode(ISD::ADD, dl, getPointerTy(DAG.getDataLayout()),
                                   StackPtr,
                                   DAG.getIntPtrConstant(VA.getLocMemOffset(), dl));


      MemOpChains.push_back(DAG.getStore(Chain, dl, Arg, PtrOff, MachinePointerInfo()));
    }
  }

  // Transform all store nodes into one single node because all store nodes are
  // independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, MemOpChains);

  // Build a sequence of copy-to-reg nodes chained together with token chain and
  // flag operands which copy the outgoing args into registers.  The InFlag in
  // necessary since all emitted instructions must be stuck together.
  SDValue InFlag;
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i) {
    Chain = DAG.getCopyToReg(Chain, dl, RegsToPass[i].first,
                             RegsToPass[i].second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl, MVT::i32);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first,
                                  RegsToPass[i].second.getValueType()));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  // attach machine-level aliasing information
  int FI = DAG.getMachineFunction().getFrameInfo().CreateFixedObject(4, 0, true);
  MachinePointerInfo MPO = MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI);
  MachineMemOperand *MMO = 
      DAG.getMachineFunction().getMachineMemOperand(MPO,
	                                            MachineMemOperand::MOLoad,
					            4, Align(1));

  Chain = DAG.getMemIntrinsicNode(PatmosISD::CALL, dl,
                                  NodeTys, Ops,
                                  MVT::i32, MMO);

//   Chain = DAG.getNode(PatmosISD::CALL, dl, NodeTys, &Ops[0], Ops.size());
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain,
                             DAG.getConstant(NumBytes, dl, getPointerTy(DAG.getDataLayout()), true),
                             DAG.getConstant(0, dl, getPointerTy(DAG.getDataLayout()), true),
                             InFlag, dl);
  InFlag = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InFlag, CallConv, isVarArg, Ins, dl,
                         DAG, InVals);
}

/// LowerCallResult - Lower the result values of a call into the
/// appropriate copies out of appropriate physical registers.
///
SDValue
PatmosTargetLowering::LowerCallResult(SDValue Chain, SDValue InFlag,
                                      CallingConv::ID CallConv, bool isVarArg,
                                      const SmallVectorImpl<ISD::InputArg> &Ins,
                                      SDLoc dl, SelectionDAG &DAG,
                                      SmallVectorImpl<SDValue> &InVals) const {

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), RVLocs, *DAG.getContext());

  CCInfo.AnalyzeCallResult(Ins, RetCC_Patmos);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    assert((RVLocs[i].getLocReg() == Patmos::R1 ||
            RVLocs[i].getLocReg() == Patmos::R2) && "Invalid return register");
    // We only support i32 return registers, so we copy from i32, no matter what
    // the actual return type in RVLocs[i].getValVT() is.
    SDValue val = DAG.getCopyFromReg( Chain, dl, RVLocs[i].getLocReg(), MVT::i32, InFlag);
    Chain = val.getValue(1);
    InFlag = val.getValue(2);

    if(RVLocs[i].getValVT() == MVT::i1) {
      // Returned i1's are returned in R1 and therefore need to be "extracted"
      // by truncating it down to i1 again
      val = DAG.getZExtOrTrunc(val, dl, RVLocs[i].getValVT());
    }
    InVals.push_back(val);
  }

  return Chain;
}

PatmosTargetLowering::ConstraintType PatmosTargetLowering::
getConstraintType(StringRef Constraint) const
{
  // Patmos specific constrains
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
      default : break;
      case 'R':
      case 'S':
      case 'P':
        return C_RegisterClass;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass*> PatmosTargetLowering::
getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                             StringRef Constraint, MVT VT) const
{
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'R':  // r0-r31
    case 'r':  // general purpose registers
        return std::make_pair(0U, &Patmos::RRegsRegClass);
    case 'S':
      if (VT == MVT::i32) {
        return std::make_pair(0U, &Patmos::SRegsRegClass);
      }
      assert("Unexpected register type");
      return std::make_pair(0U, static_cast<const TargetRegisterClass*>(0));
    case 'P':
      if (VT == MVT::i1) {
        return std::make_pair(0U, &Patmos::PRegsRegClass);
      }
      assert("Unexpected register type");
      return std::make_pair(0U, static_cast<const TargetRegisterClass*>(0));
    }
  }
  // Previously, '{$rx}' was allowed as a constraint.
  // Use of '$' preceding the register is not allowed now.
  // This ensures a sensible error message is printed if anyone accidentally does it.
  if (Constraint.size() > 2 && Constraint[0] == '{' && Constraint[1] == '$') {
    report_fatal_error("Inline assembly clobbers cannot have '$' preceding clobbered registers");
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

SDValue
PatmosTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  PatmosMachineFunctionInfo &PMFI = *MF.getInfo<PatmosMachineFunctionInfo>();

  // get VarArgsFI, i.e., the FI used to access the variadic parameters of the 
  // current function
  SDLoc dl(Op);
  SDValue VarArgsFI = DAG.getFrameIndex(PMFI.getVarArgsFI(), getPointerTy(DAG.getDataLayout()));

  // get the VarArgsFI and store it to the given address.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), // chain
                      dl,
                      VarArgsFI, // VarArgsFI
                      Op.getOperand(1), // destination address
                      MachinePointerInfo(SV));
}

const char *PatmosTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return NULL;
  case PatmosISD::RET_FLAG:           return "PatmosISD::RET_FLAG";
  case PatmosISD::CALL:               return "PatmosISD::CALL";
  case PatmosISD::MUL:                return "PatmosISD::MUL";
  case PatmosISD::MULU:               return "PatmosISD::MULU";
  }
}
