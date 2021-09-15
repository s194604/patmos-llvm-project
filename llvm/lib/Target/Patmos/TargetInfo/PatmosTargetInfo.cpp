//===-- PatmosTargetInfo.cpp - Patmos Target Implementation ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PatmosTargetInfo.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Compiler.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;


void llvm::getMBBIRName(const MachineBasicBlock *MBB,
                         SmallString<128> &result) {
  const BasicBlock *BB = MBB->getBasicBlock();
  const Twine bbname = (BB && BB->hasName()) ? BB->getName()
                                             : "<anonymous>";
  const Twine bb_ir_label = "#" + MBB->getParent()->getFunction().getName() +
                            "#" + bbname +
                            "#" + Twine(MBB->getNumber());
  bb_ir_label.toVector(result);
}

std::pair<int,int> llvm::getLoopBounds(const MachineBasicBlock * MBB) {
  if(MBB && MBB->getBasicBlock()) {
    if (auto loop_bound_meta = MBB->getBasicBlock()->getTerminator()->getMetadata("llvm.loop")) {
      // We ignore the first metadata operand, as it is always a self-reference
      // in "llvm.loop".
      for(int i = 1, end = loop_bound_meta->getNumOperands(); i < end; i++) {
        auto meta_op_node = dyn_cast<llvm::MDNode>(loop_bound_meta->getOperand(i).get());
        if( meta_op_node->getNumOperands() >= 1 ){
          auto name = cast_or_null<MDString>(meta_op_node->getOperand(0));
          auto min_node = cast_or_null<ValueAsMetadata>(meta_op_node->getOperand(1))->getValue();
          auto max_node = cast_or_null<ValueAsMetadata>(meta_op_node->getOperand(2))->getValue();
          if( name && name->getString() == "llvm.loop.bound") {
            if(min_node && max_node &&
               min_node->getType()->isIntegerTy() &&
               max_node->getType()->isIntegerTy()
             ) {
              auto min = ((llvm::ConstantInt*) min_node)->getZExtValue();
              auto max = ((llvm::ConstantInt*) max_node)->getZExtValue();

              return std::make_pair(min, max);
            } else {
              report_fatal_error(
                        "Invalid loop bounds in MBB: '" +
                        MBB->getName() + "'!");
            }
          }
        }
      }
    }
  }
  return std::make_pair(-1, -1);
}


Target &llvm::getThePatmosTarget() {
  static Target ThePatmosTarget;
  return ThePatmosTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePatmosTargetInfo() {
  RegisterTarget<Triple::patmos> X(getThePatmosTarget(), "patmos",
                                   "Patmos [experimental]", "Patmos");
}
