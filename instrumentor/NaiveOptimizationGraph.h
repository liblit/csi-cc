//===--------------------- NaiveOptimizationGraph.h -----------------------===//
//
// A very naive optimization graph implementation for locally-optimal coverage.
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
#ifndef CSI_NAIVE_OPT_GRAPH_H
#define CSI_NAIVE_OPT_GRAPH_H

#include "CoverageOptimizationGraph.h"

#include <llvm/Analysis/BlockFrequencyInfo.h>

#include <set>

namespace csi_inst {

// ---------------------------------------------------------------------------
// NaiveOptimizationGraph is a very naive optimization graph implementation
// that simply does locally-optimal optimization over the LLVM function (rather
// than building any sort of optimal graph structure or using tricks).
// ---------------------------------------------------------------------------
class NaiveOptimizationGraph : public CoverageOptimizationGraph {
private:
  // obtain a locally-optimal set of coverage probes
  std::set<llvm::BasicBlock*> locallyOptimal(
     const std::set<llvm::BasicBlock*>& I,
     const std::set<llvm::BasicBlock*>& D,
     const std::set<llvm::BasicBlock*>& X) const;

public:
  // constructor for an empty graph
  NaiveOptimizationGraph();
  // constructor from an LLVM function and block frequency info
  NaiveOptimizationGraph(llvm::Function* F, const llvm::BlockFrequencyInfo& bf);
  // TODO: constructor from an LLVM function, with unit costs
  ~NaiveOptimizationGraph();

  // compute the locally-optimal approximation of coverage probes
  std::set<llvm::BasicBlock*> getOptimizedProbes(
     const std::set<llvm::BasicBlock*>& canProbe,
     const std::set<llvm::BasicBlock*>& wantData,
     const std::set<llvm::BasicBlock*>& crashPoints) const;
};

} // end csi_inst namespace

#endif
