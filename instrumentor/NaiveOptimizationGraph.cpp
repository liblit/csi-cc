//===-------------------- NaiveOptimizationGraph.cpp ----------------------===//
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
#define DEBUG_TYPE "coverage-optimization"

#include "NaiveOptimizationGraph.h"
#include "NaiveCoverageSet.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>

#include "llvm_proxy/CFG.h"

using namespace csi_inst;
using namespace llvm;
using namespace std;


set<BasicBlock*> NaiveOptimizationGraph::locallyOptimal(
     const set<BasicBlock*>& I,
     const set<BasicBlock*>& D,
     const set<BasicBlock*>& X) const {
  if(D.size() == 0)
    return(set<BasicBlock*>());
  BasicBlock* e = this->getEntryBlock();

  // for now...though it's possible to find the apropriate subset of D to start
  DEBUG(dbgs() << "0 / " << to_string(I.size()) << '\n');
  //assert(isCoverageSet(I, D, e, X));

  set<BasicBlock*> S = I;
  vector<BasicBlock*> tryRemove = sortBlocksByCost(I);
  int __attribute__((unused)) count = 0;
  for(vector<BasicBlock*>::const_iterator i = tryRemove.begin(), ie = tryRemove.end(); i != ie; ++i){
    S.erase(*i);
    DEBUG(dbgs() << to_string(++count) << " / " << to_string(I.size()) << '\n');
    DEBUG(dbgs() << "trying to remove " << (*i)->getName().str() << "...\n");
    if(!isCoverageSetClose(S, D, e, X)){
      DEBUG(dbgs() << '\'' << (*i)->getName().str() << "' refuted close\n");
      S.insert(*i);
      continue;
    }
    if(!isCoverageSet(S, D, e, X)){
      DEBUG(dbgs() << '\'' << (*i)->getName().str() << "' refuted far\n");
      S.insert(*i);
    }
    else{
      DEBUG(dbgs() << "OK! Removed '" << (*i)->getName().str() << "'\n");
    }
  }

  // print result...if debugging the pass
  DEBUG(dbgs() << e->getParent()->getName().str()
               << " Result: " << setBB_asstring(S) << '\n');
  return(S);
}


NaiveOptimizationGraph::NaiveOptimizationGraph() : CoverageOptimizationGraph() {
  // nothing more to do
}

NaiveOptimizationGraph::NaiveOptimizationGraph(
     llvm::Function* F,
     const llvm::BlockFrequencyInfo& bf) : CoverageOptimizationGraph(F, bf) {
  // nothing more to do
}

NaiveOptimizationGraph::~NaiveOptimizationGraph(){
  // nothing more to do
}

set<BasicBlock*> NaiveOptimizationGraph::getOptimizedProbes(
     const set<BasicBlock*>& canProbe,
     const set<BasicBlock*>& wantData,
     const set<BasicBlock*>& crashPoints) const {
  return(locallyOptimal(canProbe, wantData, crashPoints));
}
