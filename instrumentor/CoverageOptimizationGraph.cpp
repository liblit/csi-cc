//===------------------ CoverageOptimizationGraph.cpp ---------------------===//
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

#include "CoverageOptimizationGraph.h"
#include "NaiveCoverageSet.h"

#include "llvm_proxy/CFG.h"
#include "Utils.hpp"
#include "Versions.h"

#include <iostream>

using namespace csi_inst;
using namespace llvm;
using namespace std;

CoverageOptimizationGraph::EdgesT& CoverageOptimizationGraph::getEdges(){
  return(fwdEdges);
}

// a comparator for "costs" of basic blocks (currently: as given by
// BlockFrequencyInfo, and copied in the constructor)
struct BlockCostComparator{
  const CoverageOptimizationGraph* graph;

  BlockCostComparator(const CoverageOptimizationGraph* graph){
    this->graph = graph;
  }

  bool operator()(llvm::BasicBlock* first, llvm::BasicBlock* second){
    // Name ordering here is somewhat arbitrary, but preserved for
    // backward compatibility with ASE 2016 paper results.
    // When used with high compiler optimization levels,
    // we cannot assert that blocks have non-empty names.
    string firstName = first->getName().str();
    string secondName = second->getName().str();

    double firstCost = graph->getBlockCost(first);
    double secondCost = graph->getBlockCost(second);
    if(firstCost == secondCost)
      return (firstName > secondName);
    return (firstCost > secondCost);
  }
};

vector<BasicBlock*> CoverageOptimizationGraph::sortBlocksByCost(
     const set<BasicBlock*>& blocks) const {
  vector<BasicBlock*> result(blocks.begin(), blocks.end());
  sort(result.begin(), result.end(), BlockCostComparator(this));
  return(result);
}

void CoverageOptimizationGraph::fillInNodeCost(
     const llvm::BlockFrequencyInfo& bf) {
  blockCost.clear();

  const Function* graphFunction = this->function;
  if(!graphFunction)
    report_fatal_error("invalid function entry detected while attempting "
                       "to compute BB costs in coverage opt graph");

#if LLVM_VERSION < 30500
  uint64_t freqScaleInt =
     bf.getBlockFreq(&graphFunction->getEntryBlock()).getFrequency();
#else
  uint64_t freqScaleInt = bf.getEntryFreq();
#endif
  double freqScaleDouble = (double)freqScaleInt;
  
  for(Function::const_iterator i = graphFunction->begin(), e = graphFunction->end(); i != e; ++i){
    // get the cost for the basic block
    // NOTE: computed to reduce precision loss due to casts
    uint64_t myFreq = bf.getBlockFreq(&*i).getFrequency();
    uint64_t myWhole = myFreq / freqScaleInt;
    uint64_t myPart = myFreq % freqScaleInt;
    double myScaledFreq = (double)myWhole + (double)myPart / freqScaleDouble;
    blockCost[&*i] = myScaledFreq;
  }
}

void CoverageOptimizationGraph::buildGraphFromFunction(Function* F){
  this->fwdEdges.clear();

  for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
    vector<BasicBlock*>& edges = fwdEdges[&*i];
    for(succ_iterator s = succ_begin(&*i), se = succ_end(&*i); s != se; ++s)
      edges.push_back(*s);
  }
  assert(fwdEdges.size() == F->size());
}

CoverageOptimizationGraph::CoverageOptimizationGraph(){
  // do nothing, thereby making an empty graph
  this->function = NULL;
  this->entryBlock = NULL;
}

CoverageOptimizationGraph::CoverageOptimizationGraph(
     llvm::Function* F,
     const llvm::BlockFrequencyInfo& bf){
  this->function = F;
  this->entryBlock = &F->getEntryBlock();
  fillInNodeCost(bf);

  buildGraphFromFunction(F);
}

CoverageOptimizationGraph::~CoverageOptimizationGraph(){
  // nothing to do (no heap allocations)
}

void CoverageOptimizationGraph::printGraph(raw_ostream &out) const {
  for(EdgesT::const_iterator i = fwdEdges.begin(), e = fwdEdges.end(); i != e; ++i){
    const vector<BasicBlock*>& edges = i->second;

    out << i->first->getName().str() << " -> {";
    for(vector<BasicBlock*>::const_iterator i = edges.begin(), e = edges.end(); i != e; ++i)
      out << " " << (*i)->getName().str();
    out << " }\n";
  }
}

double CoverageOptimizationGraph::getBlockCost(const BasicBlock* block) const {
  return(blockCost.at(block));
}

Function* CoverageOptimizationGraph::getFunction() const {
  return(function);
}

BasicBlock* CoverageOptimizationGraph::getEntryBlock() const {
  return(entryBlock);
}

const vector<BasicBlock*>& CoverageOptimizationGraph::getBlockSuccs(BasicBlock* block) const {
  return(fwdEdges.at(block));
}
