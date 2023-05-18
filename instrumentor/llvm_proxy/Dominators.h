//===------------------------- llvm_proxy/Dominators.h --------------------===//
//
// A proxy header file to cleanly support different versions of LLVM.
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

#ifndef INCLUDE_LLVM_PROXY_DOMINATORS_H
#define INCLUDE_LLVM_PROXY_DOMINATORS_H

#include "../Versions.h"

#if LLVM_VERSION < 30500
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #include <llvm/Analysis/Dominators.h>
  #pragma GCC diagnostic pop

inline void addRequiredDominatorTree(llvm::AnalysisUsage &usage) {
  usage.addRequired<llvm::DominatorTree>();
}

inline llvm::DominatorTree &getDominatorTree(llvm::Pass &pass, llvm::Function &function) {
  return pass.getAnalysis<llvm::DominatorTree>(function);
}

inline llvm::DominatorTree &getDominatorTree(llvm::Pass &pass) {
  return pass.getAnalysis<llvm::DominatorTree>();
}

#else
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #include <llvm/IR/Dominators.h>
  #pragma GCC diagnostic pop

inline void addRequiredDominatorTree(llvm::AnalysisUsage &usage) {
  usage.addRequired<llvm::DominatorTreeWrapperPass>();
}

inline llvm::DominatorTree &getDominatorTree(llvm::Pass &pass, llvm::Function &function) {
  return pass.getAnalysis<llvm::DominatorTreeWrapperPass>(function).getDomTree();
}

inline llvm::DominatorTree &getDominatorTree(llvm::Pass &pass) {
  return pass.getAnalysis<llvm::DominatorTreeWrapperPass>().getDomTree();
}
#endif

#endif	// !INCLUDE_LLVM_PROXY_DOMINATORS_H
