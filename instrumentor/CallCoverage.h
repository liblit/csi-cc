//===-------------------------- CallCoverage.h ----------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
// gathering both global and local coverage information.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2016 Peter J. Ohmann and Benjamin R. Liblit
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

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

namespace csi_inst {

// ---------------------------------------------------------------------------
// CallCoverage is a module pass which does call coverage instrumentation
// ---------------------------------------------------------------------------
class CallCoverage : public LocalCoveragePass {
private:
  static Options options;

  // Perform module-level tasks, open streams, and instrument each function
  bool runOnModule(llvm::Module &M);
  
  // Write out the information for one call within a function
  void writeOneCall(llvm::CallInst* theCall, unsigned int index,
                    bool isInstrumented=true);
  
  // Instrument each function for coverage on each call
  void instrumentFunction(llvm::Function &);
  
public:
  static const CoveragePassNames names;
  static char ID; // Pass identification, replacement for typeid
  CallCoverage() : LocalCoveragePass(ID, names) {}

  virtual const char *getPassName() const {
    return "Intra/Interprocedural Call Coverage Instrumentation";
  }
};
} // end csi_inst namespace

#endif
