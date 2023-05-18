//===------------------------ NaiveCoverageSets.h -------------------------===//
//
// A very naive implementation of checking coverage sets.
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
#ifndef CSI_NAIVE_COVERAGE_SET_H
#define CSI_NAIVE_COVERAGE_SET_H

#include "llvm_proxy/BasicBlock.h"

#include <set>

namespace csi_inst {

// determine if a particular set is a coverage set of the specified desired
// nodes
bool isCoverageSet(const std::set<llvm::BasicBlock*>& S,
                   const std::set<llvm::BasicBlock*>& D,
                   llvm::BasicBlock* e,
                   const std::set<llvm::BasicBlock*>& X);

// determine if a particular set is a coverage set, considering only the closest
// alphas and betas (WARNING: a result of true does *not* necessarily fully mean
// that S is a coverage set of D!)
bool isCoverageSetClose(const std::set<llvm::BasicBlock*>& S,
                        const std::set<llvm::BasicBlock*>& D,
                        llvm::BasicBlock* e,
                        const std::set<llvm::BasicBlock*>& X);

std::set<llvm::BasicBlock*> firstEncountered(
     llvm::BasicBlock* from,
     const std::set<llvm::BasicBlock*>& to,
     bool forward);

std::set<llvm::BasicBlock*> firstTwoEncountered(
     llvm::BasicBlock* from,
     const std::set<llvm::BasicBlock*>& to,
     bool forward);

void oneHop(
     llvm::BasicBlock* from,
     const std::set<llvm::BasicBlock*>& to,
     bool forward,
     std::set<llvm::BasicBlock*>& result);

// determine if an "ambiguous triangle" exists between a particular alpha,
// beta, and desired node
bool hasAmbiguousTriangle(llvm::BasicBlock* alpha,
                          llvm::BasicBlock* beta,
                          llvm::BasicBlock* d,
                          llvm::BasicBlock* e,
                          const std::set<llvm::BasicBlock*>& X,
                          const std::set<llvm::BasicBlock*>& S);

// determine if a path exists from a node in "from" to a node in "to" without
// passing through any nodes in "excluding"
bool isConnectedExcluding(const std::set<llvm::BasicBlock*>& from,
                          const std::set<llvm::BasicBlock*>& to,
                          const std::set<llvm::BasicBlock*>& excluding);

  // determine all nodes reachable along any path from a node in "from" to a
  // node in "to" without passing through any nodes in "excluding"
std::set<llvm::BasicBlock*> connectedExcluding(
     const std::set<llvm::BasicBlock*>& from,
     const std::set<llvm::BasicBlock*>& to,
     const std::set<llvm::BasicBlock*>& excluding);

} // end csi_inst namespace

#endif
