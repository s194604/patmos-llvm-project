//===- PatmosRegisterInfo.h - Patmos Register Information Impl --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Patmos implementation of the MRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef _LLVM_TARGET_PATMOS_REGISTERINFO_H_
#define _LLVM_TARGET_PATMOS_REGISTERINFO_H_

#define GET_REGINFO_HEADER
#include "PatmosGenRegisterInfo.inc"

namespace llvm {

class TargetInstrInfo;
class PatmosTargetMachine;

struct PatmosRegisterInfo : public PatmosGenRegisterInfo {
private:
  const PatmosTargetMachine &TM;
  const TargetInstrInfo &TII;

  /// computeLargeFIOffset - Emit an ADDi or ADDl instruction to compute a large
  /// FI offset.
  /// \note The offset and basePtr arguments are possibly updated!
  void computeLargeFIOffset(MachineRegisterInfo &MRI,
                            int &offset, unsigned &basePtr,
                            MachineBasicBlock::iterator II,
                            int shl) const;

  /// expandPseudoPregInstr - expand PSEUDO_PREG_SPILL or PSEUDO_PREG_RELOAD
  /// to a sequence of real machine instructions.
  void expandPseudoPregInstr(MachineBasicBlock::iterator II,
                             int offset, unsigned basePtr,
                             bool isOnStackCache) const;
public:
  PatmosRegisterInfo(const PatmosTargetMachine &tm, const TargetInstrInfo &tii);

  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;

  /// get the associate patmos target machine
  const PatmosTargetMachine& getTargetMachine() const { return TM; }

  const MCPhysReg* getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;


  const TargetRegisterClass *
  getMatchingSuperRegClass(const TargetRegisterClass *A,
                           const TargetRegisterClass *B, unsigned Idx) const override {
    // No sub-classes makes this really easy.
    return A;
  }

  bool isRReg(unsigned RegNo) const;
  bool isSReg(unsigned RegNo) const;
  bool isPReg(unsigned RegNo) const;

  /// getS0Index - Returns the index of a given PReg in S0; -1 if RegNo is
  /// not a PReg
  /// e.g. getS0Index(Patmos::P0) -> 0
  ///      getS0Index(Patmos::P1) -> 1
  ///      getS0Index(Patmos::P2) -> 2
  ///      getS0Index(Patmos::R9) -> -1
  int getS0Index(unsigned RegNo) const;

  bool hasReservedSpillSlot(const MachineFunction &MF, Register Reg,
                                    int &FrameIdx) const override;

  bool requiresRegisterScavenging(const MachineFunction &MF) const override;

  bool requiresFrameIndexScavenging(const MachineFunction &MF) const override {
    return false; //FIXME
  }

  bool trackLivenessAfterRegAlloc(const MachineFunction &MF) const override {
    return true;
  }

  void eliminateFrameIndex(MachineBasicBlock::iterator II,
                           int SPAdj, unsigned FIOperandNum,
		                   RegScavenger *RS = nullptr) const override;

  // Debug information queries.
  Register getFrameRegister(const MachineFunction &MF) const override;

  bool isConstantPhysReg(MCRegister PhysReg) const override;

  const TargetRegisterClass *
  getPointerRegClass(const MachineFunction &MF,
                     unsigned Kind = 0) const override {
    return &Patmos::RRegsRegClass;
  }

};

/// Used purely to provide an easy way of printing registers with the '<<' operator.
/// Use: outs() << "Register: " << PrintReg(Register, TRI) << "\n";
struct PrintReg {
  PrintReg(Register R, const TargetRegisterInfo &I)
    : Rs(R), HRI(I) {}
  Register Rs;
  const TargetRegisterInfo &HRI;
};

LLVM_ATTRIBUTE_UNUSED
raw_ostream &operator<< (raw_ostream &OS, const PrintReg &P);

} // end namespace llvm

#endif // _LLVM_TARGET_PATMOS_REGISTERINFO_H_
