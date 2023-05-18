//===------------------- CoverageOptimizationGraph.h ----------------------===//
//
// A superclass for all classes that do dominator-based or locally-optimal
// coverage optimization (over the CFG).  Does not compute or store dominator
// information.
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
#ifndef CSI_COVERAGE_OPT_GRAPH_H
#define CSI_COVERAGE_OPT_GRAPH_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/Analysis/BlockFrequencyInfo.h>
#pragma GCC diagnostic pop

#include <map>
#include <set>
#include <vector>

namespace csi_inst {


// ---------------------------------------------------------------------------
// CoverageOptimizationGraph is a the superclass for all graph classes used
// for coverage optimization
// ---------------------------------------------------------------------------
class CoverageOptimizationGraph{
protected:
  // forward and backward edge type
  typedef std::map<llvm::BasicBlock*, std::vector<llvm::BasicBlock*> > EdgesT;

  // get a NO-LONGER-const version of the edges in the graph
  EdgesT& getEdges();

  // sort the provided blocks by their cost
  std::vector<llvm::BasicBlock*> sortBlocksByCost(
       const std::set<llvm::BasicBlock*>& blocks) const;

private:
  // the internal graph representation (not necessarily that used by subclasses)
  EdgesT fwdEdges;

  // the function upon which this graph was built
  llvm::Function* function;

  // the entry block
  llvm::BasicBlock* entryBlock;

  // a local cache of the estimated cost of each node
  std::map<const llvm::BasicBlock*, double> blockCost;

  // fill in the cost of each block
  void fillInNodeCost(const llvm::BlockFrequencyInfo& bf);

  // fill in the edges and block/ID maps based on the llvm function
  void buildGraphFromFunction(llvm::Function* F);

public:
  // constructor for an empty graph
  CoverageOptimizationGraph();
  // constructor from an LLVM function and block frequency info
  CoverageOptimizationGraph(llvm::Function* F, const llvm::BlockFrequencyInfo& bf);
  // TODO: constructor from an LLVM function, with unit costs
  virtual ~CoverageOptimizationGraph();

  // print this graph in human-readable form
  virtual void printGraph(llvm::raw_ostream &) const;

  // compute the locally-optimal approximation of coverage probes
  virtual std::set<llvm::BasicBlock*> getOptimizedProbes(
     const std::set<llvm::BasicBlock*>& canProbe,
     const std::set<llvm::BasicBlock*>& wantData,
     const std::set<llvm::BasicBlock*>& crashPoints) const = 0;

  // return the estimated "cost" of a BB in the graph
  double getBlockCost(const llvm::BasicBlock* block) const;

  // return the function upon which this graph was built
  llvm::Function* getFunction() const;

  // return the entry block in the graph
  llvm::BasicBlock* getEntryBlock() const;

  // return the successors for the provided block in the graph
  const std::vector<llvm::BasicBlock*>& getBlockSuccs(llvm::BasicBlock* block) const;
};

} // end csi_inst namespace

#endif
