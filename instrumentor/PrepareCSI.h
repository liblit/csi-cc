//===--------------------------- PrepareCSI.h -----------------------------===//
//
// This module pass replicates functions to allow multiple possible
// instrumentation schemes.  Note that, presently, this causes enormous code
// bloat.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2023 Peter J. Ohmann and Benjamin R. Liblit
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===----------------------------------------------------------------------===//
#ifndef CSI_INSTRUMENTATION_H
#define CSI_INSTRUMENTATION_H

#include "PassName.h"

#include <llvm/Pass.h>

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

#include <set>
#include <map>
#include <string>
#include <vector>

namespace csi_inst {

// ---------------------------------------------------------------------------
// PrepareCSI is a module pass that analyzes each function, and prepares
// each for the appropriate types of instrumentation.
// ---------------------------------------------------------------------------
class PrepareCSI : public llvm::ModulePass {
private:
  // Current context for multi threading support.
  llvm::LLVMContext* Context;
  
#if LLVM_VERSION < 30300
  // instrumentation attributes attached to each function (so instrumentors
  // know which functions to operate on)
  std::map<const llvm::Function*, std::set<std::string> > functionSchemes;
#endif
  
  // functions to create the trampoline function
  // NOTE: this returns a function which is now F (but as a trampoline).  The
  // return value may or may not be F, and, in fact, it is possible after this
  // call that F no longer exists.  All future references to F should be changed
  // to the return value.  The function will, however, fix all existing
  // references to F (in the bitcode).
  llvm::Function* switchIndirect(llvm::Function* F,
                                 llvm::GlobalVariable* switcher,
                                 std::vector<llvm::Function*>& replicas);
  llvm::Function* ifuncIndirect(llvm::Function* F,
                                llvm::GlobalVariable* switcher,
                                std::vector<llvm::Function*>& replicas);
  
  // Analyzes all functions, duplicates, and creates the dispatcher
  bool runOnModule(llvm::Module &M);

public:
  static char ID; // Pass identification, replacement for typeid
  PrepareCSI() : ModulePass(ID) {}

  virtual PassName getPassName() const {
    return "CSI Preparation for Instrumentation";
  }
  
  // does the specified function require the specified instrumentation?
  bool hasInstrumentationType(const llvm::Function &, const std::string &type) const;
  
private:
  // mark the specified function as requiring the specified instrumentation
  void addInstrumentationType(llvm::Function &F, const std::string &type);
};
} // end csi_inst namespace

#endif
