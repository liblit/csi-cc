//===---------------------------- Utils.hpp -------------------------------===//
//
// Utilities providing convenience functions for various tasks not specific to
// any particular instrumentation pass.
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

#ifndef CSI_UTILS
#define CSI_UTILS

#include "Versions.h"

#include "llvm_proxy/BasicBlock.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/DIBuilder.h"
#include "llvm_proxy/Module.h"

#include <set>
#include <string>

namespace llvm {
  class AllocaInst;
  class ArrayType;
  class DIBasicType;
  class DILocalVariable;
  class ModulePass;
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

  llvm::DebugLoc findEarlyDebugLoc(const llvm::Function &, bool silent);

#if LLVM_VERSION < 30600
  llvm::DIVariable createAutoVariable(llvm::DIBuilder &, llvm::DIDescriptor, llvm::StringRef, llvm::DIFile, unsigned, llvm::DIType, bool = false);
#elif LLVM_VERSION < 30700
  llvm::DIVariable createAutoVariable(llvm::DIBuilder &, llvm::DIDescriptor, llvm::StringRef, llvm::DIFile, unsigned, llvm::DITypeRef, bool = false);
#else
  llvm::DILocalVariable *createAutoVariable(llvm::DIBuilder &, llvm::DIScope *, llvm::StringRef, llvm::DIFile *, unsigned, llvm::DIType *, bool = false);
#endif


#if LLVM_VERSION < 30700

  // get or create a global variable with the specified parameters
  llvm::GlobalVariable &getOrCreateGlobal(llvm::DIBuilder &, llvm::Function &,
                                          llvm::Type &, const llvm::DIType &,
                                          const std::string &upperShortName);

  llvm::DIType createArrayType(llvm::DIBuilder &, uint64_t count, const llvm::DIType &elementType);

  void createGlobalVariable(llvm::DIBuilder &, const llvm::DIType &, llvm::GlobalVariable &);

  inline bool isUnknown(const llvm::DebugLoc &location)
  {
    return location.isUnknown();
  }

  llvm::Instruction *insertDeclare(llvm::DIBuilder &, llvm::Value *, llvm::DIVariable, const llvm::DebugLoc &, llvm::Instruction *);

  llvm::AllocaInst *createZeroedLocalArray(llvm::Function &, llvm::ArrayType &, const std::string &name, llvm::DIBuilder &, const llvm::DIType &, bool);

#else

  inline bool isUnknown(const llvm::DebugLoc &location)
  {
    return !location;
  }

  // get or create a global variable with the specified parameters
  llvm::GlobalVariable &getOrCreateGlobal(llvm::DIBuilder &, llvm::Function &,
                                          llvm::Type &, llvm::DIType *,
                                          const std::string &upperShortName);

  llvm::DIType *createArrayType(llvm::DIBuilder &, uint64_t count, llvm::DIType *elementType);

  void createGlobalVariable(llvm::DIBuilder &, llvm::DIType *, llvm::GlobalVariable &);

  llvm::Instruction *insertDeclare(llvm::DIBuilder &, llvm::Value *, llvm::DILocalVariable *, const llvm::DebugLoc &, llvm::Instruction *);

  llvm::AllocaInst *createZeroedLocalArray(llvm::Function &, llvm::ArrayType &, const std::string &name, llvm::DIBuilder &, llvm::DIType *, bool);
#endif

  void attachCSILabelToInstruction(llvm::Instruction&,
                                   const std::string& label);

  // We don't yet require C++11, so we'll use our own "to_string" functions
  std::string to_string(int val);
  std::string to_string(unsigned int val);
  std::string to_string(long unsigned int val);
  std::string to_string(double val, unsigned int digits=2);

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


void createCompileUnit(llvm::DIBuilder &, const llvm::Module &, const llvm::ModulePass &);


inline
#if LLVM_VERSION < 30700
const llvm::DIType
#else
llvm::DIBasicType *
#endif
createBasicType(llvm::DIBuilder &builder, llvm::StringRef name, uint64_t sizeInBits, unsigned encoding)
{
#if LLVM_VERSION < 40000
  return builder.createBasicType(name, sizeInBits, sizeInBits, encoding);
#else
  return builder.createBasicType(name, sizeInBits, encoding);
#endif
}


#if LLVM_VERSION < 40000
#define CL_ENUM_VAL_END , clEnumValEnd
#else
#define CL_ENUM_VAL_END
#endif


#endif
