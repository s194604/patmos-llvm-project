//===--- Patmos.cpp - Implement RISCV target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Patmos TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Patmos.h"
#include "clang/Basic/MacroBuilder.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/TargetParser.h"

using namespace clang;
using namespace clang::targets;

ArrayRef<const char *> PatmosTargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      // CPU register names
      // Must match second column of GCCRegAliases
      // The names here must match the register enum names in the .td file,
      // not the register name string value (case insensitive).
      "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
      "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
      "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
      "r24",  "r25",  "r26",  "rtr",  "rfp",  "rsp",  "rfb",  "rfo",
      // Predicates
      "p0",  "p1",  "p2",  "p3",  "p4",  "p5",  "p6",  "p7",
      // Special registers
      "s0",  "sm",  "sl",  "sh",  "s4",  "ss",  "st",  "s7",
      "s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15"
  };
  return llvm::makeArrayRef(GCCRegNames);
}

ArrayRef<TargetInfo::GCCRegAlias> PatmosTargetInfo::getGCCRegAliases() const {
  static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
      { { "r27" }, "rtr" },
      { { "r28" }, "rfp" },
      { { "r29" }, "rsp" },
      { { "r30" }, "rfb" },
      { { "r31" }, "rfo" },
      { { "s1"  }, "sm"  },
      { { "s2"  }, "sl"  },
      { { "s3"  }, "sh"  },
      { { "s5"  }, "ss"  },
      { { "s6"  }, "st"  }
  };
  return llvm::makeArrayRef(GCCRegAliases);
}

bool PatmosTargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  // clang actually accepts a few generic register constraints more (i,n,m,o,g,..),
  // not much we can do about it.. For completeness, we list all currently supported
  // constraints here.

  switch (*Name) {
    default:
      return false;

    case 'r': // CPU registers.
    // TODO do not accept read-only or special registers here
    case 'R': // r0-r31, currently same as 'r'
    case 'S': // sz-s15
    case 'P': // p0-p7
    // TODO define more classes for subsets of registers (r10-r28, ..)?
      Info.setAllowsRegister();
      return true;
    case '{':
      Name++;
      if (!*Name)
        return false;
      Name++;
      while (*Name) {
        if (*Name == '}') {
          return true;
        }
        if (*Name != 'r' && *Name != 's' && *Name != 'p' &&
            (*Name < '0' || *Name > '9')) {
          return false;
        }
        Name++;
      }
      return false;
  }
}

void PatmosTargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  // Target identification.
  Builder.defineMacro("__patmos__");
  Builder.defineMacro("__PATMOS__");

  if (SoftFloat)
    Builder.defineMacro("SOFT_FLOAT", "1");
}

/// Return true if has this feature, need to sync with handleTargetFeatures.
bool PatmosTargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
              .Case("softfloat", SoftFloat)
              .Case("patmos", true)
              .Default(false);
}

/// Perform initialization based on the user configured set of features.
bool PatmosTargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                           DiagnosticsEngine &Diags) {
  SoftFloat = true;
  for (unsigned i = 0, e = Features.size(); i != e; ++i) {
    if (Features[i] == "+hard-float")
      SoftFloat = false;
    if (Features[i] == "+soft-float")
      SoftFloat = true;
  }
  return true;
}
