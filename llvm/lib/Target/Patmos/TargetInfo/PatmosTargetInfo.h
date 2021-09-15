//===-- PatmosTargetInfo.h - Patmos Target Implementation -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PATMOS_TARGETINFO_PATMOSTARGETINFO_H
#define LLVM_LIB_TARGET_PATMOS_TARGETINFO_PATMOSTARGETINFO_H

#include "llvm/IR/Function.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/ADT/SmallString.h"

namespace llvm {

class Target;

Target &getThePatmosTarget();

/// getMBBIRName - Get a name for the MBB that reflects its identity in the
/// LLVM IR. If there is no mapping from IR code, an <anonymous> is used.
/// For uniqueness, the function name and current MBB number are included.
/// Format: #FunctionName#BasicBlockName#MBBNumber
void getMBBIRName(const MachineBasicBlock *MBB,
                         SmallString<128> &result);

/// Extracts loop bound information from the metadata of the block
/// terminator, if available.
///
/// The first element is the minimum iteration count.
/// The second element is the maximum iteration count.
/// If a bound is not available, -1 is returned.
std::pair<int,int> getLoopBounds(const MachineBasicBlock * MBB);

} // namespace llvm

#endif // LLVM_LIB_TARGET_PATMOS_TARGETINFO_PATMOSTARGETINFO_H
