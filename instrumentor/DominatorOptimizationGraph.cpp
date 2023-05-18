//===------------------ DominatorOptimizationGraph.cpp --------------------===//
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
#define DEBUG_TYPE "coverage-optimization"

#include "DominatorOptimizationGraph.h"

#include <llvm/Support/Debug.h>

#include <stack>

using namespace csi_inst;
using namespace llvm;
using namespace std;


void DominatorOptimizationGraph::recAddToGraph(const DomTreeNode* node) {
  BasicBlock* block = node->getBlock();
  if(block == NULL)
    report_fatal_error("adding NULL node to dominator tree");
  nodes.insert(block);

  set<BasicBlock *>& successors = tree[block];
  const vector<DomTreeNode*>& children = node->getChildren();
  for(vector<DomTreeNode*>::const_iterator i = children.begin(), e = children.end(); i != e; ++i){
    successors.insert((*i)->getBlock());
    recAddToGraph(*i);
  }
}

// return all basic blocks reachable in the dominator tree, in a reverse
// topological order
vector<BasicBlock*> DominatorOptimizationGraph::reverseTopo() const {
  vector<BasicBlock*> order;

  set<BasicBlock*> temporaryMark;
  set<BasicBlock*> mark;
  for(set<BasicBlock*>::iterator i = nodes.begin(), e = nodes.end(); i != e; ++i){
    recReverseTopo(*i, &order, &temporaryMark, &mark);
  }

  return(order);
}

void DominatorOptimizationGraph::recReverseTopo(BasicBlock* node,
                                                vector<BasicBlock*>* order,
                                                set<BasicBlock*>* temporaryMark,
                                                set<BasicBlock*>* mark) const {
  if(mark->count(node))
    return;
  else if(temporaryMark->count(node))
    report_fatal_error("a non-DAG was constructed for the dominator tree "
                       "for function '" + node->getParent()->getName() + "'");
  else if(!tree.count(node))
    report_fatal_error("topological sort encountered a non-existant node");

  // temporarily mark, and recursively visit children
  temporaryMark->insert(node);
  const set<BasicBlock*>& children = this->getChildren(node);
  for(set<BasicBlock*>::iterator i = children.begin(), e = children.end(); i != e; ++i)
    recReverseTopo(*i, order, temporaryMark, mark);

  // make sure there was no problem, mark node as completed,
  // and add it into the order
  if(mark->count(node))
    report_fatal_error("node was processed twice in topological order for "
                       "function '" + node->getParent()->getName() + "'");

  mark->insert(node);
  order->push_back(node);
}

bool DominatorOptimizationGraph::exitWithout(
     BasicBlock* node,
     const set<BasicBlock*>& exits,
     const set<BasicBlock*>& without) const{
  set<BasicBlock*> visited;
  stack<BasicBlock*> worklist;

  visited.insert(node);
  visited.insert(without.begin(), without.end());
  worklist.push(node);
  while(!worklist.empty()){
    BasicBlock* cur = worklist.top();
    worklist.pop();
    visited.insert(cur);
    if(succ_begin(cur) == succ_end(cur))
      return(true);
    else if(cur != node && exits.count(cur) && !dominates(node, cur))
      return(true);
    for(succ_iterator i = succ_begin(cur), e = succ_end(cur); i != e; ++i){
      if(!visited.count(*i))
        worklist.push(*i);
    }
  }
  return(false);
}

void DominatorOptimizationGraph::coverNode(BasicBlock* node,
                                           const set<BasicBlock*>& canCover,
                                           const set<BasicBlock*>& canInst,
                                           const set<BasicBlock*>& exits,
                                           set<BasicBlock*>* willInst,
                                           set<BasicBlock*>* willCover) const {
  // find out which dominator children can possibly be covered
  set<BasicBlock*> canCoverChildren;
  const set<BasicBlock*>& children = this->getChildren(node);
  for(set<BasicBlock*>::iterator i = children.begin(), e = children.end(); i != e; ++i){
    if(canCover.count(*i))
      canCoverChildren.insert(*i);
  }

  if(exitWithout(node, exits, canCoverChildren)){
    // if all cover-able children aren't enough to cover "node", then we have
    // to instrument "node"
    if(!canInst.count(node))
      report_fatal_error("assertion violated while trying to cover node: expectation that node '" + node->getName() + "' could be instrumented was false; please report this");
    willInst->insert(node);
  }
  else{
    // otherwise, cover the cheapest set of dominator children in order to
    // cover "node"
    set<BasicBlock*> cheapChildren = cheapestChildren(node,
                                                      canCover,
                                                      *willCover,
                                                      exits);
    for(set<BasicBlock*>::iterator i = cheapChildren.begin(), e = cheapChildren.end(); i != e; ++i)
      coverNode(*i, canCover, canInst, exits, willInst, willCover);
  }
  
  willCover->insert(node);
}

set<BasicBlock*> DominatorOptimizationGraph::cheapestChildren(
     BasicBlock* node,
     const set<BasicBlock*>& canCover,
     const set<BasicBlock*>& willCover,
     const set<BasicBlock*>& exits) const {
  // assert that *all* dominator children are sufficient to cover "node"
  // (a precondition of this function)
  set<BasicBlock*> canCoverChildren;
  const set<BasicBlock*>& children = this->getChildren(node);
  for(set<BasicBlock*>::iterator i = children.begin(), e = children.end(); i != e; ++i){
    if(canCover.count(*i))
      canCoverChildren.insert(*i);
  }
  if(exitWithout(node, exits, canCoverChildren))
    report_fatal_error("attempt to find cheapest child set for node that "
                       "cannot be covered by children");

  vector<BasicBlock*> removableChildren = sortBlocksByCost(canCoverChildren);
  for(vector<BasicBlock*>::iterator child = removableChildren.begin(), e = removableChildren.end(); child != e; ++child){
    if(willCover.count(*child))
      continue;

    set<BasicBlock*> otherChildren = canCoverChildren;
    otherChildren.erase(*child);
    if(!exitWithout(node, exits, otherChildren))
      canCoverChildren.erase(*child);
  }

  return(canCoverChildren);
}

bool DominatorOptimizationGraph::dominates(BasicBlock* dominator,
                                           BasicBlock* dominated) const {
  const set<BasicBlock*>& children = tree.at(dominator);
  if(children.count(dominated))
    return(true);

  for(set<BasicBlock*>::const_iterator i = children.begin(), e = children.end(); i != e; ++i){
    if(this->dominates(*i, dominated))
      return(true);
  }

  return(false);
}

const set<BasicBlock*>& DominatorOptimizationGraph::getChildren(
     BasicBlock* node) const {
  const Optimization_Tree::const_iterator found = tree.find(node);
  if (found == tree.end())
    report_fatal_error("request for children of node not in dominator tree");
  return found->second;
}







DominatorOptimizationGraph::DominatorOptimizationGraph() :
     CoverageOptimizationGraph() {
  // nothing more to do
}

DominatorOptimizationGraph::DominatorOptimizationGraph(
     Function* F,
     const BlockFrequencyInfo& bf,
     const DominatorTree& domTree) : CoverageOptimizationGraph(F, bf) {
  const DomTreeNode* domRoot = domTree.getRootNode();
  recAddToGraph(domRoot);
}

DominatorOptimizationGraph::~DominatorOptimizationGraph(){
  // nothing more to do
}

set<BasicBlock*> DominatorOptimizationGraph::getOptimizedProbes(
     const set<BasicBlock*>& canProbe,
     const set<BasicBlock*>& wantData,
     const set<BasicBlock*>& crashPoints) const {
  vector<BasicBlock*> reverseTopoOrdering = reverseTopo();

  set<BasicBlock*> canCover; // = canInst || !needInst
  set<BasicBlock*> needInst; // = algorithm(children | canCover)
  for(vector<BasicBlock*>::iterator i = reverseTopoOrdering.begin(), e = reverseTopoOrdering.end(); i != e; ++i){
    set<BasicBlock*> coveredChildren;
    const set<BasicBlock*>& children = this->getChildren(*i);
    for(set<BasicBlock*>::iterator j = children.begin(), je = children.end(); j != je; ++j){
      if(canCover.count(*j))
        coveredChildren.insert(*j);
    }

    bool myCanInst = canProbe.count(*i);
    bool myNeedInst = exitWithout(*i, crashPoints, coveredChildren); // = algorithm(children | canCover)
    bool myCanCover = !myNeedInst || myCanInst;

    if(myCanCover)
      canCover.insert(*i);
    if(myNeedInst)
      needInst.insert(*i);
  }

  // assert that we can get a coverage set...otherwise give up
  vector<BasicBlock*> uncovered;
  set_difference(wantData.begin(), wantData.end(),
                 canCover.begin(), canCover.end(),
                 std::back_inserter(uncovered));
  if(!uncovered.empty())
    return(canProbe);

  set<BasicBlock*> willInst; // the set of nodes we will instrument
                             // w/o wantData => needInst && canInst

  set<BasicBlock*> willCover; // the set of nodes we will end up covering

  for(vector<BasicBlock*>::iterator i = reverseTopoOrdering.begin(), e = reverseTopoOrdering.end(); i != e; ++i){
    set<BasicBlock*> willCoverChildren;
    const set<BasicBlock*> &children = this->getChildren(*i);
    for(set<BasicBlock*>::iterator j = children.begin(), je = children.end(); j != je; ++j){
      if(willCover.count(*j))
        willCoverChildren.insert(*j);
    }
    if(!exitWithout(*i, crashPoints, willCoverChildren))
      willCover.insert(*i);
    if(!wantData.count(*i))
      continue;

    if(needInst.count(*i) && canProbe.count(*i)){
      willInst.insert(*i);
      willCover.insert(*i);
    }
    else if(!needInst.count(*i)){
      coverNode(*i, canCover, canProbe, crashPoints, &willInst, &willCover);
      willCover.insert(*i);
    }
    else{
      DEBUG(dbgs() << "Cannot cover requested block '"
      << (*i)->getName().str() << "' based on instrumentation "
      << "restrictions\n");
    }
  }

  uncovered.clear();
  set_difference(wantData.begin(), wantData.end(),
                 willCover.begin(), willCover.end(),
                 std::back_inserter(uncovered));
  if(!uncovered.empty())
    report_fatal_error("didn't coverage what we expected to cover in "
                       "dominator optimization");

  return(willInst);
}
