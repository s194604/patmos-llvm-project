//==- PatmosFrameLowering.h - Define frame lowering for Patmos --*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _PATMOS_FRAMEINFO_H_
#define _PATMOS_FRAMEINFO_H_

#include "Patmos.h"
#include "PatmosSubtarget.h"
#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class PatmosFrameLowering : public TargetFrameLowering {
protected:
  const PatmosSubtarget &STC;

  /// getEffectiveStackCacheSize - Return the size of the stack cache that can
  /// be used by the compiler.
  /// \see EnableBlockAlignedStackCache
  unsigned getEffectiveStackCacheSize() const;

  /// getEffectiveStackCacheBlockSize - Return the size of the stack cache's 
  /// blocks as seen from the instruction set architecture.
  /// \see EnableBlockAlignedStackCache
  unsigned getEffectiveStackCacheBlockSize() const;

  /// getAlignedStackCacheFrameSize - Return the frame size aligned to the 
  /// effective stack cache block size.
  /// \see EnableBlockAlignedStackCache
  /// \see getEffectiveStackCacheBlockSize
  unsigned getAlignedStackCacheFrameSize(unsigned frameSize) const;

  /// assignFIsToStackCache - Assign some FIs to the stack cache.
  /// Currently this is only done for spill slots.
  /// @param SCFIs - should be set to true for all indices of frame objects
  ///                that should be assigned to the stack cache.
  void assignFIsToStackCache(MachineFunction &MF, BitVector &SCFIs) const;

  /// assignFrameObjects - Fix the layout of the stack frame, assign FIs to
  /// either stack cache or shadow stack, and update all stack offsets.
  /// Also reserves space for the call frame if no frame pointer is used.
  /// @return The final size of the shadow stack.
  unsigned assignFrameObjects(MachineFunction &MF, bool UseStackCache) const;

  /// emitSTC - Emit a stack reserve/free/ensure operation.
  /// The size of the stack frame is calculated before by assignFIsToStackCache
  /// and is retrieved via the PatmosMachineFunctionInfo.
  /// \see assignFIsToStackCache
  /// \see PatmosMachineFunctionInfo
  /// @return The emitted instruction, or NULL if no instruction was emitted.
  MachineInstr *emitSTC(MachineFunction &MF, MachineBasicBlock &MBB,
               MachineBasicBlock::iterator &MI, unsigned Opcode) const;

  /// patchCallSites - Emit stack ensure operations after every call.
  /// The size of the stack frame is calculated before by assignFIsToStackCache
  /// and is retrieved via the PatmosMachineFunctionInfo.
  /// \see assignFIsToStackCache
  /// \see PatmosMachineFunctionInfo
  void patchCallSites(MachineFunction &MF) const;
public:
  explicit PatmosFrameLowering(const PatmosSubtarget &sti, const DataLayout* DL)
        : TargetFrameLowering(StackGrowsDown, DL->getStackAlignment(), 0), STC(sti) {
    }

  /// emitProlog/emitEpilog - These methods insert prolog and epilog code into
  /// the function.
  void emitPrologue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;

  bool hasFP(const MachineFunction &MF) const override;

  /// processFunctionBeforeCalleeSavedScan - This method is called immediately
  /// before PrologEpilogInserter scans the physical registers used to determine
  /// what callee saved registers should be spilled. This method is optional.
  virtual void processFunctionBeforeCalleeSavedScan(MachineFunction &MF,
                                                 RegScavenger *RS = NULL) const;
  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;
  bool restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI,
                                   MutableArrayRef<CalleeSavedInfo> CSI,
                                   const TargetRegisterInfo *TRI) const override;
  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF,
                                MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I) const override;

};

} // End llvm namespace

#endif // _PATMOS_FRAMEINFO_H_
