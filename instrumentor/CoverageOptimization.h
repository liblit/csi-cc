//===---------------------- CoverageOptimization.h ------------------------===//
//
// This pass gathers information for coverage optimization, and
// exposes an interface for callers to ask for customized, optimized coverage.
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
#ifndef CSI_COVERAGE_OPT_H
#define CSI_COVERAGE_OPT_H

#include "CoverageOptimizationGraph.h"
#include "DominatorOptimizationGraph.h"
#include "PassName.h"

#include <llvm/Analysis/BlockFrequencyInfo.h>
#include <llvm/Pass.h>

#include "llvm_proxy/Dominators.h"

#include <map>
#include <set>

namespace csi_inst {

// ---------------------------------------------------------------------------
// CoverageOptimizationData is a FunctionPass based on the function's CFG that
// stores pre-computed data involved in coverage optimization.  Currently, it
// only stores costs of BBs.
// ---------------------------------------------------------------------------
class CoverageOptimizationData : public llvm::FunctionPass {
private:
  // auto_ptr is deprecated in c++11, but unique_ptr doesn't exist in c++03
  std::auto_ptr<CoverageOptimizationGraph> graph;
  DominatorOptimizationGraph tree;

  // build the optimization data for this function
  bool runOnFunction(llvm::Function &F);

#if defined(USE_GAMS) || defined(USE_LEMON)
  // the full variant of coverage optimization.  Calls out to the GAMS
  // optimization framework
  std::set<llvm::BasicBlock*> getOptimizedProbes_full(
     std::set<llvm::BasicBlock*>* canProbe,
     std::set<llvm::BasicBlock*>* wantData,
     std::set<llvm::BasicBlock*>* crashPoints) const;
#endif
  
  // a locally-optimal approximation to the global optimal result for coverage
  // probes
  std::set<llvm::BasicBlock*> getOptimizedProbes_cheap(
     std::set<llvm::BasicBlock*>* canProbe,
     std::set<llvm::BasicBlock*>* wantData,
     std::set<llvm::BasicBlock*>* crashPoints) const;
  
public:
  static char ID; // Pass identification, replacement for typeid
  CoverageOptimizationData() : FunctionPass(ID) {}

  virtual PassName getPassName() const {
    return "CSI coverage optimization analysis";
  }
  
  void getAnalysisUsage(llvm::AnalysisUsage &) const;
  
  // Based on the pre-computed information, get the optimal set of basic blocks
  // to instrument.
  // I, if not NULL, indicates the legal set of blocks to select (i.e.
  // only allows the optimization to choose from a subset of the blocks)
  // D, if not NULL, only optimizes for those "desired" blocks.
  // fullOptimization, if true, will use the GAMS modeling system to solve the
  // "complete" optimization problem.  If false, use a locally-optimal
  // approximation.
  // The result of calling this function is a set of optimal blocks.
  std::set<llvm::BasicBlock*> getOptimizedProbes(llvm::Function* F,
     std::set<llvm::BasicBlock*>* I = NULL,
     std::set<llvm::BasicBlock*>* D = NULL
#if defined(USE_GAMS) || defined(USE_LEMON)
     , bool fullOptimization = false
#endif
  ) const;
};

} // end csi_inst namespace

#endif
