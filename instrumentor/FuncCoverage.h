//===-------------------------- FuncCoverage.h ----------------------------===//
//
// This pass instruments function entry points for interprocedural
// analysis by gathering global function coverage information.
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
#ifndef CSI_FN_COVERAGE_H
#define CSI_FN_COVERAGE_H

#include "CoveragePass.h"
#include "PassName.h"

#include <llvm/Pass.h>

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

#include <map>
#include <set>
#include <fstream>

namespace csi_inst {

// ---------------------------------------------------------------------------
// FuncCoverage is a module pass which does function coverage instrumentation
// ---------------------------------------------------------------------------
class FuncCoverage : public CoveragePass {
private:
  static Options options;
  
  // Perform module-level tasks, open streams, and instrument each function
  bool runOnModule(llvm::Module &M);

  // Instrument each function with coverage at entry
  void instrumentFunction(llvm::Function &, llvm::DIBuilder &);
  
public:
  static const CoveragePassNames names;
  static char ID; // Pass identification, replacement for typeid
  FuncCoverage() : CoveragePass(ID, names) {}

  virtual PassName getPassName() const {
    return "Interprocedural Function Coverage Instrumentation";
  }
};
} // end csi_inst namespace

#endif
