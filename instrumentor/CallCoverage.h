//===-------------------------- CallCoverage.h ----------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
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
#ifndef CSI_CALL_COVERAGE_H
#define CSI_CALL_COVERAGE_H

#include "ExtrinsicCalls.h"
#include "LocalCoveragePass.h"
#include "PassName.h"

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

#include <map>
#include <set>
#include <fstream>

namespace csi_inst {

// ---------------------------------------------------------------------------
// CallCoverage is a module pass which does call coverage instrumentation
// ---------------------------------------------------------------------------
class CallCoverage : public LocalCoveragePass {
private:
  static Options options;

  // the mapping from functions to the list of instrumented call-sites
  std::map<llvm::Function*, std::set<llvm::CallInst*> > functionCalls;
  
  // select one call instruction for each basic block chosen to instrument
  // NOTE: each basic block *must* have at least one call; this is an error,
  // and is not ignored
  std::set<llvm::CallInst*> selectCalls(const std::set<llvm::BasicBlock*>& bbs);
  // get the set of basic blocks containing the call instructions (the resulting
  // set may be smaller if some calls share basic blocks)
  std::set<llvm::BasicBlock*> getBBsForCalls(
     const std::set<llvm::CallInst*>& calls);
  
  // Perform module-level tasks, open streams, and instrument each function
  bool runOnModule(llvm::Module &M);
  
  // Write out the information for one call within a function
  void writeOneCall(llvm::CallInst* theCall, unsigned int index,
                    bool isInstrumented=true);
  
  // Instrument each function for coverage on each call
  void instrumentFunction(llvm::Function &, llvm::DIBuilder &debugBuilder);
  
public:
  static const CoveragePassNames names;
  static char ID; // Pass identification, replacement for typeid
  CallCoverage() : LocalCoveragePass(ID, names) {}

  virtual PassName getPassName() const {
    return "Intra/Interprocedural Call Coverage Instrumentation";
  }
};
} // end csi_inst namespace

#endif
