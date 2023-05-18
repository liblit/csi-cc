//===--------------------------- BBCoverage.h -----------------------------===//
//
// This pass instruments basic blocks for interprocedural analysis by
// gathering both global and local coverage information.
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
#ifndef CSI_BB_COVERAGE_H
#define CSI_BB_COVERAGE_H

#include "LocalCoveragePass.h"
#include "PassName.h"


#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

#include <map>
#include <set>
#include <fstream>

namespace llvm {
  class DIBuilder;
}

namespace csi_inst {

// ---------------------------------------------------------------------------
// BBCoverage is a module pass which does basic block coverage instrumentation
// ---------------------------------------------------------------------------
class BBCoverage : public LocalCoveragePass {
private:
  static Options options;
  
  // Perform module-level tasks, open streams, and instrument each function
  bool runOnModule(llvm::Module &M);
  
  // Write out the information for one basic block within a function
  void writeOneBB(llvm::BasicBlock* theBB, unsigned int index,
                  bool isInstrumented=true);
  
  std::set<llvm::BasicBlock*> getOptimizedInstrumentation(llvm::Function& F);
  
  // Instrument each function with coverage for each basic block
  void instrumentFunction(llvm::Function &, llvm::DIBuilder &debugBuilder);

public:
  static const CoveragePassNames names;
  static char ID; // Pass identification, replacement for typeid
  BBCoverage() : LocalCoveragePass(ID, names) {}

  virtual PassName getPassName() const {
    return "Intra/Interprocedural Basic Block Coverage Instrumentation";
  }

  void getAnalysisUsage(llvm::AnalysisUsage &) const;
};
} // end csi_inst namespace

#endif
