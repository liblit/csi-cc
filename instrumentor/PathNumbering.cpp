//===------------------------ PathNumbering.cpp ---------------------------===//
//
// Ball-Larus path numbers uniquely identify paths through a directed acyclic
// graph (DAG) [1].  For a CFG backedges are removed and replaced by phony
// edges to obtain a DAG, and thus the unique path numbers.  The purpose of
// this analysis is to enumerate the edges in a CFG in order to obtain paths
// from path numbers in a convenient manner.
//
// [1]
//  T. Ball and J. R. Larus. "Efficient Path Profiling."
//  International Symposium on Microarchitecture, pages 46-57, 1996.
//  http://portal.acm.org/citation.cfm?id=243857
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
//
//                      The LLVM Compiler Infrastructure
//
// This file is derived from the file PathNumbering.cpp, originally distributed
// with the LLVM Compiler Infrastructure, and was originally licensed under the
// University of Illinois Open Source License.  See UI_LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "PP-ball-larus-numbering"

#include "PathNumbering.h"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm_proxy/Instructions.h"
#include "llvm_proxy/TypeBuilder.h"

#include <queue>
#include <string>
#include <utility>
#include <sstream>

using namespace csi_inst;
using namespace llvm;

// Returns the basic block for the PPBallLarusNode
BasicBlock* PPBallLarusNode::getBlock() {
  return(_basicBlock);
}

// Returns the number of paths to the exit starting at the node.
unsigned long PPBallLarusNode::getNumberPaths() {
  return(_numberPaths);
}

// Sets the number of paths to the exit starting at the node.
void PPBallLarusNode::setNumberPaths(unsigned long numberPaths) {
  _numberPaths = numberPaths;
}

// Gets the NodeColor used in graph algorithms.
PPBallLarusNode::NodeColor PPBallLarusNode::getColor() {
  return(_color);
}

// Sets the NodeColor used in graph algorithms.
void PPBallLarusNode::setColor(PPBallLarusNode::NodeColor color) {
  _color = color;
}

// Returns an iterator over predecessor edges. Includes phony and
// backedges.
PPBLEdgeIterator PPBallLarusNode::predBegin() {
  return(_predEdges.begin());
}

// Returns the end sentinel for the predecessor iterator.
PPBLEdgeIterator PPBallLarusNode::predEnd() {
  return(_predEdges.end());
}

// Returns the number of predecessor edges.  Includes phony and
// backedges.
unsigned PPBallLarusNode::getNumberPredEdges() {
  return(_predEdges.size());
}

// Returns an iterator over successor edges. Includes phony and
// backedges.
PPBLEdgeIterator PPBallLarusNode::succBegin() {
  return(_succEdges.begin());
}

// Returns the end sentinel for the successor iterator.
PPBLEdgeIterator PPBallLarusNode::succEnd() {
  return(_succEdges.end());
}

// Returns the number of successor edges.  Includes phony and
// backedges.
unsigned PPBallLarusNode::getNumberSuccEdges() {
  return(_succEdges.size());
}

// Add an edge to the predecessor list.
void PPBallLarusNode::addPredEdge(PPBallLarusEdge* edge) {
  _predEdges.push_back(edge);
}

// Remove an edge from the predecessor list.
void PPBallLarusNode::removePredEdge(PPBallLarusEdge* edge) {
  removeEdge(_predEdges, edge);
}

// Add an edge to the successor list.
void PPBallLarusNode::addSuccEdge(PPBallLarusEdge* edge) {
  _succEdges.push_back(edge);
}

// Remove an edge from the successor list.
void PPBallLarusNode::removeSuccEdge(PPBallLarusEdge* edge) {
  removeEdge(_succEdges, edge);
}

// Returns the name of the BasicBlock being represented.  If BasicBlock
// is null then returns "<null>".  If BasicBlock has no name, then
// "<unnamed>" is returned.  Intended for use with debug output.
std::string PPBallLarusNode::getName() {
  std::stringstream name;

  if(getBlock() != NULL) {
    if(getBlock()->hasName()) {
      std::string tempName(getBlock()->getName());
      name << tempName.c_str() << " (" << _uid << ')';
    } else
      name << "<unnamed> (" << _uid << ')';
  } else
    name << "<null> (" << _uid << ')';

  return name.str();
}

// Removes an edge from an edgeVector.  Used by removePredEdge and
// removeSuccEdge.
void PPBallLarusNode::removeEdge(PPBLEdgeVector& v, PPBallLarusEdge* e) {
  // TODO: Avoid linear scan by using a set instead
  for(PPBLEdgeIterator i = v.begin(),
        end = v.end();
      i != end;
      ++i) {
    if((*i) == e) {
      v.erase(i);
      break;
    }
  }
}

// Returns the source node of this edge.
PPBallLarusNode* PPBallLarusEdge::getSource() const {
  return(_source);
}

// Returns the target node of this edge.
PPBallLarusNode* PPBallLarusEdge::getTarget() const {
  return(_target);
}

// Sets the type of the edge.
PPBallLarusEdge::EdgeType PPBallLarusEdge::getType() const {
  return _edgeType;
}

// Gets the type of the edge.
void PPBallLarusEdge::setType(EdgeType type) {
  _edgeType = type;
}

// Returns the weight of this edge.  Used to decode path numbers to sequences
// of basic blocks.
unsigned long PPBallLarusEdge::getWeight() {
  return(_weight);
}

// Sets the weight of the edge.  Used during path numbering.
void PPBallLarusEdge::setWeight(unsigned long weight) {
  _weight = weight;
}

// Gets the phony edge originating at the root.
PPBallLarusEdge* PPBallLarusEdge::getPhonyRoot() {
  return _phonyRoot;
}

// Sets the phony edge originating at the root.
void PPBallLarusEdge::setPhonyRoot(PPBallLarusEdge* phonyRoot) {
  _phonyRoot = phonyRoot;
}

// Gets the phony edge terminating at the exit.
PPBallLarusEdge* PPBallLarusEdge::getPhonyExit() {
  return _phonyExit;
}

// Sets the phony edge terminating at the exit.
void PPBallLarusEdge::setPhonyExit(PPBallLarusEdge* phonyExit) {
  _phonyExit = phonyExit;
}

// Gets the associated real edge if this is a phony edge.
PPBallLarusEdge* PPBallLarusEdge::getRealEdge() {
  return _realEdge;
}

// Sets the associated real edge if this is a phony edge.
void PPBallLarusEdge::setRealEdge(PPBallLarusEdge* realEdge) {
  _realEdge = realEdge;
}

// Returns the duplicate number of the edge.
unsigned PPBallLarusEdge::getDuplicateNumber() {
  return(_duplicateNumber);
}

// Initialization that requires virtual functions which are not fully
// functional in the constructor.
void PPBallLarusDag::init() {
  PPBLBlockNodeMap inDag;
  std::stack<PPBallLarusNode*> dfsStack;

  _root = addNode(&(_function.getEntryBlock()));
  _exit = addNode(NULL);

  // start search from root
  dfsStack.push(getRoot());

  // dfs to add each bb into the dag
  while(dfsStack.size())
    buildNode(inDag, dfsStack);

  // put in the final edge
  addEdge(getExit(),getRoot(),0);
}

// Frees all memory associated with the DAG.
PPBallLarusDag::~PPBallLarusDag() {
  for(PPBLEdgeIterator edge = _edges.begin(), end = _edges.end(); edge != end;
      ++edge)
    delete (*edge);

  for(PPBLNodeIterator node = _nodes.begin(), end = _nodes.end(); node != end;
      ++node)
    delete (*node);
}

// Calculate the path numbers by assigning edge increments as prescribed
// in Ball-Larus path profiling.
void PPBallLarusDag::calculatePathNumbers() {
  PPBallLarusNode* node;
  std::queue<PPBallLarusNode*> bfsQueue;
  bfsQueue.push(getExit());

  while(!bfsQueue.empty()) {
    node = bfsQueue.front();

    DEBUG(dbgs() << "calculatePathNumbers on " << node->getName() << '\n');

    bfsQueue.pop();
    unsigned long prevPathNumber = node->getNumberPaths();
    calculatePathNumbersFrom(node);

    // Check for DAG splitting
    //if( node->getNumberPaths() > 100000000 && node != getRoot() ) {
    if (node->getNumberPaths() > (ULONG_MAX / 4) && node != getRoot()){
      DEBUG(dbgs() << "WARNING: DAG splitting occurred for function "
                   << getFunction().getName().str() << '\n');
      
      // Add new phony edge from the split-node to the DAG's exit
      PPBallLarusEdge* exitEdge = addEdge(node, getExit(), 0);
      exitEdge->setType(PPBallLarusEdge::SPLITEDGE_PHONY);

      // Counters to handle the possibility of a multi-graph
      BasicBlock* oldTarget = 0;
      unsigned duplicateNumber = 0;

      // Iterate through each successor edge, adding phony edges
      for( PPBLEdgeIterator succ = node->succBegin(), end = node->succEnd();
           succ != end; oldTarget = (*succ)->getTarget()->getBlock(), succ++ ) {

        if( (*succ)->getType() == PPBallLarusEdge::NORMAL ) {
          // is this edge a duplicate?
          if( oldTarget != (*succ)->getTarget()->getBlock() )
            duplicateNumber = 0;

          // create the new phony edge: root -> succ
          PPBallLarusEdge* rootEdge =
            addEdge(getRoot(), (*succ)->getTarget(), duplicateNumber++);
          rootEdge->setType(PPBallLarusEdge::SPLITEDGE_PHONY);
          rootEdge->setRealEdge(*succ);

          // split on this edge and reference it's exit/root phony edges
          (*succ)->setType(PPBallLarusEdge::SPLITEDGE);
          (*succ)->setPhonyRoot(rootEdge);
          (*succ)->setPhonyExit(exitEdge);
          (*succ)->setWeight(0);
        }
      }

      calculatePathNumbersFrom(node);
    }

    DEBUG(dbgs() << "prev, new number paths " << prevPathNumber << ", "
          << node->getNumberPaths() << ".\n");

    if(prevPathNumber == 0 && node->getNumberPaths() != 0) {
      DEBUG(dbgs() << "node ready : " << node->getName() << '\n');
      for(PPBLEdgeIterator pred = node->predBegin(), end = node->predEnd();
          pred != end; pred++) {
        if( (*pred)->getType() == PPBallLarusEdge::BACKEDGE ||
            (*pred)->getType() == PPBallLarusEdge::SPLITEDGE )
          continue;

        PPBallLarusNode* nextNode = (*pred)->getSource();
        // not yet visited?
        if(nextNode->getNumberPaths() == 0)
          bfsQueue.push(nextNode);
      }
    }
  }

  DEBUG(dbgs() << "\tNumber of paths: " << getRoot()->getNumberPaths() << '\n');
}

// Returns the number of paths for the Dag.
unsigned long PPBallLarusDag::getNumberOfPaths() {
  return(getRoot()->getNumberPaths());
}

// Returns the root (i.e. entry) node for the DAG.
PPBallLarusNode* PPBallLarusDag::getRoot() {
  return _root;
}

// Returns the exit node for the DAG.
PPBallLarusNode* PPBallLarusDag::getExit() {
  return _exit;
}

// Returns the function for the DAG.
Function& PPBallLarusDag::getFunction() {
  return(_function);
}

// Clears the node colors.
void PPBallLarusDag::clearColors(PPBallLarusNode::NodeColor color) {
  for (PPBLNodeIterator nodeIt = _nodes.begin(); nodeIt != _nodes.end(); nodeIt++)
    (*nodeIt)->setColor(color);
}

// Processes one node and its imediate edges for building the DAG.
void PPBallLarusDag::buildNode(PPBLBlockNodeMap& inDag, PPBLNodeStack& dfsStack) {
  PPBallLarusNode* currentNode = dfsStack.top();
  BasicBlock* currentBlock = currentNode->getBlock();

  if(currentNode->getColor() != PPBallLarusNode::WHITE) {
    // we have already visited this node
    dfsStack.pop();
    currentNode->setColor(PPBallLarusNode::BLACK);
  } else {
    TerminatorInst* terminator = currentNode->getBlock()->getTerminator();
    if(isa<ReturnInst>(terminator) || isa<UnreachableInst>(terminator) ||
       isa<ResumeInst>(terminator))
      addEdge(currentNode, getExit(),0);

    currentNode->setColor(PPBallLarusNode::GRAY);
    inDag[currentBlock] = currentNode;

    BasicBlock* oldSuccessor = 0;
    unsigned duplicateNumber = 0;

    // iterate through this node's successors
    for(succ_iterator successor = succ_begin(currentBlock),
          succEnd = succ_end(currentBlock); successor != succEnd;
        oldSuccessor = *successor, ++successor ) {
      BasicBlock* succBB = *successor;

      // is this edge a duplicate?
      if (oldSuccessor == succBB)
        duplicateNumber++;
      else
        duplicateNumber = 0;

      buildEdge(inDag, dfsStack, currentNode, succBB, duplicateNumber);
    }
  }
}

// Process an edge in the CFG for DAG building.
void PPBallLarusDag::buildEdge(PPBLBlockNodeMap& inDag, std::stack<PPBallLarusNode*>&
                             dfsStack, PPBallLarusNode* currentNode,
                             BasicBlock* succBB, unsigned duplicateCount) {
  PPBallLarusNode* succNode = inDag[succBB];

  if(succNode && succNode->getColor() == PPBallLarusNode::BLACK) {
    // visited node and forward edge
    addEdge(currentNode, succNode, duplicateCount);
  } else if(succNode && succNode->getColor() == PPBallLarusNode::GRAY) {
    // visited node and back edge
    DEBUG(dbgs() << "Backedge detected.\n");
    addBackedge(currentNode, succNode, duplicateCount);
  } else {
    PPBallLarusNode* childNode;
    // not visited node and forward edge
    if(succNode) // an unvisited node that is child of a gray node
      childNode = succNode;
    else { // an unvisited node that is a child of a an unvisted node
      childNode = addNode(succBB);
      inDag[succBB] = childNode;
    }
    addEdge(currentNode, childNode, duplicateCount);
    dfsStack.push(childNode);
  }
}

// The weight on each edge is the increment required along any path that
// contains that edge.
void PPBallLarusDag::calculatePathNumbersFrom(PPBallLarusNode* node) {
  if(node == getExit())
    // The Exit node must be base case
    node->setNumberPaths(1);
  else {
    unsigned long sumPaths = 0;
    PPBallLarusNode* succNode;

    bool printedOverflow = false;
    
    for(PPBLEdgeIterator succ = node->succBegin(), end = node->succEnd();
        succ != end; succ++) {
      if( (*succ)->getType() == PPBallLarusEdge::BACKEDGE ||
          (*succ)->getType() == PPBallLarusEdge::SPLITEDGE )
        continue;

      (*succ)->setWeight(sumPaths);
      succNode = (*succ)->getTarget();

      if( !succNode->getNumberPaths() )
        return;
      
      unsigned long oldPaths = sumPaths;
      sumPaths += succNode->getNumberPaths();
      if(sumPaths < oldPaths || sumPaths < succNode->getNumberPaths()){
        if(!printedOverflow)
          DEBUG(dbgs() << "WARNING: edge weight overflow.  setting to max.\n");
        printedOverflow = true;
        this->_errorEdgeOverflow = true;
        sumPaths = ULONG_MAX;
      }
    }

    node->setNumberPaths(sumPaths);
  }
}

// Allows subclasses to determine which type of Node is created.
// Override this method to produce subclasses of PPBallLarusNode if
// necessary. The destructor of PPBallLarusDag will call free on each
// pointer created.
PPBallLarusNode* PPBallLarusDag::createNode(BasicBlock* BB) {
  return( new PPBallLarusNode(BB) );
}

// Allows subclasses to determine which type of Edge is created.
// Override this method to produce subclasses of PPBallLarusEdge if
// necessary. The destructor of PPBallLarusDag will call free on each
// pointer created.
PPBallLarusEdge* PPBallLarusDag::createEdge(PPBallLarusNode* source,
                                        PPBallLarusNode* target,
                                        unsigned duplicateCount) {
  return( new PPBallLarusEdge(source, target, duplicateCount) );
}

// Proxy to node's constructor.  Updates the DAG state.
PPBallLarusNode* PPBallLarusDag::addNode(BasicBlock* BB) {
  PPBallLarusNode* newNode = createNode(BB);
  _nodes.push_back(newNode);
  return( newNode );
}

// Proxy to edge's constructor. Updates the DAG state.
PPBallLarusEdge* PPBallLarusDag::addEdge(PPBallLarusNode* source,
                                     PPBallLarusNode* target,
                                     unsigned duplicateCount) {
  PPBallLarusEdge* newEdge = createEdge(source, target, duplicateCount);
  _edges.push_back(newEdge);
  source->addSuccEdge(newEdge);
  target->addPredEdge(newEdge);
  return(newEdge);
}

// Adds a backedge with its phony edges. Updates the DAG state.
void PPBallLarusDag::addBackedge(PPBallLarusNode* source, PPBallLarusNode* target,
                               unsigned duplicateCount) {
  PPBallLarusEdge* childEdge = addEdge(source, target, duplicateCount);
  childEdge->setType(PPBallLarusEdge::BACKEDGE);

  childEdge->setPhonyRoot(addEdge(getRoot(), target,0));
  childEdge->setPhonyExit(addEdge(source, getExit(),0));

  childEdge->getPhonyRoot()->setRealEdge(childEdge);
  childEdge->getPhonyRoot()->setType(PPBallLarusEdge::BACKEDGE_PHONY);

  childEdge->getPhonyExit()->setRealEdge(childEdge);
  childEdge->getPhonyExit()->setType(PPBallLarusEdge::BACKEDGE_PHONY);
  _backEdges.push_back(childEdge);
}

bool PPBallLarusDag::error_edgeOverflow(){
  return this->_errorEdgeOverflow;
}
