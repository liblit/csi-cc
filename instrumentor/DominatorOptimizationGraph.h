//===------------------- DominatorOptimizationGraph.h ---------------------===//
//
// The "simple approximation" using only dominator information to optimize
// coverage probes.  This is not even a locally-optimal approximation, but is
// very fast.
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
#ifndef CSI_DOMINATOR_OPT_GRAPH_H
#define CSI_DOMINATOR_OPT_GRAPH_H

#include "CoverageOptimizationGraph.h"

#include <llvm/Analysis/BlockFrequencyInfo.h>

#include "llvm_proxy/Dominators.h"

#include <map>
#include <set>

namespace csi_inst {

// ---------------------------------------------------------------------------
// DominatorOptimizationGraph is the "simple" approximation based on the
// dominator tree (inspired by prior work in this area).
// ---------------------------------------------------------------------------
class DominatorOptimizationGraph : public CoverageOptimizationGraph {
private:
  // the internal type of the tree
  typedef std::map<llvm::BasicBlock*,
                   std::set<llvm::BasicBlock*> > Optimization_Tree;

  // the internal representation of the dominator tree
  Optimization_Tree tree;

  // the set of nodes in the dominator tree
  std::set<llvm::BasicBlock*> nodes;

  // add all dominator edges to the tree, based on node as the root
  void recAddToGraph(const llvm::DomTreeNode* node);

  // return all nodes reachable in the dominator tree, in a reverse topological
  // order
  std::vector<llvm::BasicBlock*> reverseTopo() const;
  // internal function used by reverseTopo()
  void recReverseTopo(llvm::BasicBlock* node,
                      std::vector<llvm::BasicBlock*>* order,
                      std::set<llvm::BasicBlock*>* temporaryMark,
                      std::set<llvm::BasicBlock*>* mark) const;

  // determine if there is a path in the associated LLVM function from "node"
  // to any exit bypassing all nodes in "without"
  // NOTE: if "crashPoints" is NULL, all blocks are considered possible
  // exits/crahes
  bool exitWithout(llvm::BasicBlock* node,
                   const std::set<llvm::BasicBlock*>& crashPoints,
                   const std::set<llvm::BasicBlock*>& without) const;

  // cover "node" in whatever way necessary (whether via dominator children,
  // or by instrumenting "node" itself)
  void coverNode(llvm::BasicBlock* node,
                 const std::set<llvm::BasicBlock*>& canCover,
                 const std::set<llvm::BasicBlock*>& canInst,
                 const std::set<llvm::BasicBlock*>& crashPoints,
                 std::set<llvm::BasicBlock*>* willInst,
                 std::set<llvm::BasicBlock*>* willCover) const;

  // find the cheapest dominator children of "node" that cover "node"
  std::set<llvm::BasicBlock*> cheapestChildren(llvm::BasicBlock* node,
                       const std::set<llvm::BasicBlock*>& canCover,
                       const std::set<llvm::BasicBlock*>& willCover,
                       const std::set<llvm::BasicBlock*>& crashPoints) const;

  // check if node "dominator" dominates node "dominated"
  // NOTE: this is *not* a cheap operation, and walks the dominator tree
  bool dominates(llvm::BasicBlock* dominator, llvm::BasicBlock* dominated) const;

  // return the set of children for a given dominator tree node
  const std::set<llvm::BasicBlock*>& getChildren(llvm::BasicBlock* node) const;

public:
  // constructor for an empty graph
  DominatorOptimizationGraph();
  // constructor from an LLVM function, block frequency info
  DominatorOptimizationGraph(llvm::Function* F,
                             const llvm::BlockFrequencyInfo& bf,
                             const llvm::DominatorTree& domTree);
  // TODO: constructor from an LLVM function, with unit costs
  ~DominatorOptimizationGraph();

  // compute the approximation of coverage probes
  std::set<llvm::BasicBlock*> getOptimizedProbes(
     const std::set<llvm::BasicBlock*>& canProbe,
     const std::set<llvm::BasicBlock*>& wantData,
     const std::set<llvm::BasicBlock*>& crashPoints) const;
};

} // end csi_inst namespace

#endif
