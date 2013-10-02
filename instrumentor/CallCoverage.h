//===-------------------------- CallCoverage.h ----------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
// gathering both global and local coverage information.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2013 Peter J. Ohmann and Benjamin R. Liblit
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

#include "PathTracing.h"

#include <llvm/Pass.h>

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/DIBuilder.h"
#include "llvm_proxy/Instructions.h"

#include <map>
#include <set>
#include <fstream>

namespace csi_inst {

// ---------------------------------------------------------------------------
// CallCoverage is a function pass which does call coverage instrumentation
// ---------------------------------------------------------------------------
class CallCoverage : public llvm::FunctionPass {
private:
  // Current context for multi threading support.
  llvm::LLVMContext* Context;
  // The Debug Info Builder for this module
  llvm::DIBuilder* Builder;
  
  // Insert necessary global variables for functions to use
  bool doInitialization(llvm::Module &M);
  // Finish up by closing the info file stream
  bool doFinalization(llvm::Module& M);
  // Instrument each function with coverage stuff on each call
  bool runOnFunction(llvm::Function &F);
  // Write out the function data for the info file
  void writeFunctionValue(llvm::Function& F);
  // Write out the information for one call within a function
  void writeOneCall(llvm::CallInst* theCall, unsigned int index);
  
  std::ofstream _infoStream; // The output stream to the info file (managed
                             // by runOnFunction and written to as we go)
  
public:
  
  // the mapping from functions to the list of instrumented
  // call-sites future passes (i.e. CallCoverage) will have for that function
  std::map<llvm::Function*, std::set<llvm::CallInst*> > functionCalls;
  // the mapping from functions to their global array
  std::map<llvm::Function*, llvm::GlobalVariable*> funcToGlobalArray;
  
  static char ID; // Pass identification, replacement for typeid
  CallCoverage() : FunctionPass(ID) {}

  virtual const char *getPassName() const {
    return "Intra/Interprocedural Call Coverage Instrumentation";
  }
  
  // We need the mapping information from the path tracing instrumentation
  //void getAnalysisUsage(AnalysisUsage &AU) const {
  //  //AU.setPreservesCFG();
  //  AU.addRequired<PathProfiler>();
  //}
};
} // end csi_inst namespace

#endif
