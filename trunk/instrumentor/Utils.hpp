//===---------------------------- Utils.hpp -------------------------------===//
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

#ifndef CSI_UTILS
#define CSI_UTILS

#include "llvm_proxy/BasicBlock.h"

inline llvm::BasicBlock::iterator nextInst(llvm::Instruction* const instruction){
  return ++llvm::BasicBlock::iterator(instruction);
}


inline llvm::BasicBlock::iterator prevInst(llvm::Instruction* const instruction){
  return --llvm::BasicBlock::iterator(instruction);
}


#endif
