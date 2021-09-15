//===-- PatmosSelectionDAGInfo.h - Patmos SelectionDAG Info -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the Patmos subclass for SelectionDAGTargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef _PATMOS_SELECTIONDAGINFO_H_
#define _PATMOS_SELECTIONDAGINFO_H_

#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

namespace llvm {

class PatmosTargetMachine;

class PatmosSelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  explicit PatmosSelectionDAGInfo();
  ~PatmosSelectionDAGInfo();
};

}

#endif // _PATMOS_SELECTIONDAGINFO_H_
