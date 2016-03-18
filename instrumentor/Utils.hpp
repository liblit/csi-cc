//===---------------------------- Utils.hpp -------------------------------===//
//
// Utilities providing convenience functions for various tasks not specific to
// any particular instrumentation pass.
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

#ifndef CSI_UTILS
#define CSI_UTILS

#include "llvm_proxy/BasicBlock.h"
#include "llvm_proxy/DIBuilder.h"
#include "llvm_proxy/Module.h"

#include <set>
#include <string>

namespace llvm {
  class AllocaInst;
  class ArrayType;
}

namespace csi_inst {

inline llvm::BasicBlock::iterator nextInst(llvm::Instruction* const instruction){
  return ++llvm::BasicBlock::iterator(instruction);
}


inline llvm::BasicBlock::iterator prevInst(llvm::Instruction* const instruction){
  return --llvm::BasicBlock::iterator(instruction);
}

  // get a unique name for a C function, F (utilizes function name and filename,
  // where possible)
  std::string getUniqueCFunctionName(const llvm::Function &F);

  // debugging routine to translate a set of basic blocks into a printable string
  std::string setBB_asstring(std::set<llvm::BasicBlock*> theSet);

  // get or create a global variable with the specified parameters
  llvm::GlobalVariable &getOrCreateGlobal(llvm::DIBuilder &, llvm::Function &,
					  llvm::Type &, const llvm::DIType &,
					  const std::string &upperShortName);

  llvm::DIType createArrayType(llvm::DIBuilder &, uint64_t count, const llvm::DIType &elementType);

  llvm::DIGlobalVariable createGlobalVariable(llvm::DIBuilder &, const llvm::DIType &, llvm::GlobalVariable &);

  llvm::DebugLoc findEarlyDebugLoc(const llvm::Function &, bool silent);

  llvm::Instruction *insertDeclare(llvm::DIBuilder &, llvm::Value *, llvm::DIVariable, const llvm::DebugLoc &, llvm::Instruction *);

  llvm::AllocaInst *createZeroedLocalArray(llvm::Function &, llvm::ArrayType &, const std::string &name, llvm::DIBuilder &, const llvm::DIType &, bool);

} // end csi_inst namespace

#if LLVM_VERSION < 30300
namespace llvm {
  // the crash diagnostic parameter does not exist prior to LLVM 3.3
  // (we just discard this parameter in earlier LLVM versions and call
  // down to llvm's report_fatal_error)
  inline void report_fatal_error(const llvm::Twine &Reason, bool)
  {
    report_fatal_error(Reason);
  }
}
#endif

#endif
