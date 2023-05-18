//===-------------------------- PathNumbering.h ---------------------------===//
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

#ifndef PP_PATH_NUMBERING_H
#define PP_PATH_NUMBERING_H

#include "llvm_proxy/CFG.h"

#include <map>
#include <stack>
#include <vector>

namespace csi_inst {
class PPBallLarusNode;
class PPBallLarusEdge;
class PPBallLarusDag;

// typedefs for storage/ interators of various DAG components
typedef std::vector<PPBallLarusNode*> PPBLNodeVector;
typedef std::vector<PPBallLarusNode*>::iterator PPBLNodeIterator;
typedef std::vector<PPBallLarusEdge*> PPBLEdgeVector;
typedef std::vector<PPBallLarusEdge*>::iterator PPBLEdgeIterator;
typedef std::map<llvm::BasicBlock*, PPBallLarusNode*> PPBLBlockNodeMap;
typedef std::stack<PPBallLarusNode*> PPBLNodeStack;

// Represents a basic block with information necessary for the BallLarus
// algorithms.
class PPBallLarusNode {
public:
  enum NodeColor { WHITE, GRAY, BLACK };

  // Constructor: Initializes a new Node for the given BasicBlock
  PPBallLarusNode(llvm::BasicBlock* BB) :
    _basicBlock(BB), _numberPaths(0), _color(WHITE) {
    static unsigned nextUID = 0;
    _uid = nextUID++;
  }

  // Returns the basic block for the PPBallLarusNode
  llvm::BasicBlock* getBlock();

  // Get/set the number of paths to the exit starting at the node.
  unsigned long getNumberPaths();
  void setNumberPaths(unsigned long numberPaths);

  // Get/set the NodeColor used in graph algorithms.
  NodeColor getColor();
  void setColor(NodeColor color);

  // Iterator information for predecessor edges. Includes phony and
  // backedges.
  PPBLEdgeIterator predBegin();
  PPBLEdgeIterator predEnd();
  unsigned getNumberPredEdges();

  // Iterator information for successor edges. Includes phony and
  // backedges.
  PPBLEdgeIterator succBegin();
  PPBLEdgeIterator succEnd();
  unsigned getNumberSuccEdges();

  // Add an edge to the predecessor list.
  void addPredEdge(PPBallLarusEdge* edge);

  // Remove an edge from the predecessor list.
  void removePredEdge(PPBallLarusEdge* edge);

  // Add an edge to the successor list.
  void addSuccEdge(PPBallLarusEdge* edge);

  // Remove an edge from the successor list.
  void removeSuccEdge(PPBallLarusEdge* edge);

  // Returns the name of the BasicBlock being represented.  If BasicBlock
  // is null then returns "<null>".  If BasicBlock has no name, then
  // "<unnamed>" is returned.  Intended for use with debug output.
  std::string getName();

private:
  // The corresponding underlying BB.
  llvm::BasicBlock* _basicBlock;

  // Holds the predecessor edges of this node.
  PPBLEdgeVector _predEdges;

  // Holds the successor edges of this node.
  PPBLEdgeVector _succEdges;

  // The number of paths from the node to the exit.
  unsigned long _numberPaths;

  // 'Color' used by graph algorithms to mark the node.
  NodeColor _color;

  // Unique ID to ensure naming difference with dotgraphs
  unsigned _uid;

  // Removes an edge from an edgeVector.  Used by removePredEdge and
  // removeSuccEdge.
  void removeEdge(PPBLEdgeVector& v, PPBallLarusEdge* e);
};

// Represents an edge in the Dag.  For an edge, v -> w, v is the source, and
// w is the target.
class PPBallLarusEdge {
public:
  enum EdgeType { NORMAL, BACKEDGE, SPLITEDGE,
    BACKEDGE_PHONY, SPLITEDGE_PHONY, CALLEDGE_PHONY };

  // Constructor: Initializes an PPBallLarusEdge with a source and target.
  PPBallLarusEdge(PPBallLarusNode* source, PPBallLarusNode* target,
                                unsigned duplicateNumber)
    : _source(source), _target(target), _weight(0), _edgeType(NORMAL),
      _realEdge(NULL), _duplicateNumber(duplicateNumber) {}

  // Returns the source/ target node of this edge.
  PPBallLarusNode* getSource() const;
  PPBallLarusNode* getTarget() const;

  // Sets the type of the edge.
  EdgeType getType() const;

  // Gets the type of the edge.
  void setType(EdgeType type);

  // Returns the weight of this edge.  Used to decode path numbers to
  // sequences of basic blocks.
  unsigned long getWeight();

  // Sets the weight of the edge.  Used during path numbering.
  void setWeight(unsigned long weight);

  // Gets/sets the phony edge originating at the root.
  PPBallLarusEdge* getPhonyRoot();
  void setPhonyRoot(PPBallLarusEdge* phonyRoot);

  // Gets/sets the phony edge terminating at the exit.
  PPBallLarusEdge* getPhonyExit();
  void setPhonyExit(PPBallLarusEdge* phonyExit);

  // Gets/sets the associated real edge if this is a phony edge.
  PPBallLarusEdge* getRealEdge();
  void setRealEdge(PPBallLarusEdge* realEdge);

  // Returns the duplicate number of the edge.
  unsigned getDuplicateNumber();

protected:
  // Source node for this edge.
  PPBallLarusNode* _source;

  // Target node for this edge.
  PPBallLarusNode* _target;

private:
  // Edge weight cooresponding to path number increments before removing
  // increments along a spanning tree. The sum over the edge weights gives
  // the path number.
  unsigned long _weight;

  // Type to represent for what this edge is intended
  EdgeType _edgeType;

  // For backedges and split-edges, the phony edge which is linked to the
  // root node of the DAG. This contains a path number initialization.
  PPBallLarusEdge* _phonyRoot;

  // For backedges and split-edges, the phony edge which is linked to the
  // exit node of the DAG. This contains a path counter increment, and
  // potentially a path number increment.
  PPBallLarusEdge* _phonyExit;

  // If this is a phony edge, _realEdge is a link to the back or split
  // edge. Otherwise, this is null.
  PPBallLarusEdge* _realEdge;

  // An ID to differentiate between those edges which have the same source
  // and destination blocks.
  unsigned _duplicateNumber;
};

// Represents the Ball Larus DAG for a given Function.  Can calculate
// various properties required for instrumentation or analysis.  E.g. the
// edge weights that determine the path number.
class PPBallLarusDag {
public:
  // Initializes a PPBallLarusDag from the CFG of a given function.  Must
  // call init() after creation, since some initialization requires
  // virtual functions.
  PPBallLarusDag(llvm::Function &F)
    : _root(NULL), _exit(NULL), _errorEdgeOverflow(false), _function(F) {}

  // Initialization that requires virtual functions which are not fully
  // functional in the constructor.
  void init();

  // Frees all memory associated with the DAG.
  virtual ~PPBallLarusDag();

  // Calculate the path numbers by assigning edge increments as prescribed
  // in Ball-Larus path profiling.
  void calculatePathNumbers();

  // Returns the number of paths for the DAG.
  unsigned long getNumberOfPaths();

  // Returns the root (i.e. entry) node for the DAG.
  PPBallLarusNode* getRoot();

  // Returns the exit node for the DAG.
  PPBallLarusNode* getExit();

  // Returns the function for the DAG.
  llvm::Function& getFunction();

  // Clears the node colors.
  void clearColors(PPBallLarusNode::NodeColor color);
  
  // Whether or not the DAG is in error due to an edge weight overflow
  bool error_edgeOverflow();

protected:
  // All nodes in the DAG.
  PPBLNodeVector _nodes;

  // All edges in the DAG.
  PPBLEdgeVector _edges;

  // All backedges in the DAG.
  PPBLEdgeVector _backEdges;

  // Allows subclasses to determine which type of Node is created.
  // Override this method to produce subclasses of PPBallLarusNode if
  // necessary. The destructor of PPBallLarusDag will call free on each pointer
  // created.
  virtual PPBallLarusNode* createNode(llvm::BasicBlock* BB);

  // Allows subclasses to determine which type of Edge is created.
  // Override this method to produce subclasses of PPBallLarusEdge if
  // necessary.  Parameters source and target will have been created by
  // createNode and can be cast to the subclass of PPBallLarusNode*
  // returned by createNode. The destructor of PPBallLarusDag will call free
  // on each pointer created.
  virtual PPBallLarusEdge* createEdge(PPBallLarusNode* source, PPBallLarusNode*
                                    target, unsigned duplicateNumber);

  // Proxy to node's constructor.  Updates the DAG state.
  PPBallLarusNode* addNode(llvm::BasicBlock* BB);

  // Proxy to edge's constructor.  Updates the DAG state.
  PPBallLarusEdge* addEdge(PPBallLarusNode* source, PPBallLarusNode* target,
                         unsigned duplicateNumber);

private:
  // The root (i.e. entry) node for this DAG.
  PPBallLarusNode* _root;

  // The exit node for this DAG.
  PPBallLarusNode* _exit;
  
  // DAG in an error state with an edge weight overflow?
  bool _errorEdgeOverflow;

  // The function represented by this DAG.
  llvm::Function& _function;

  // Processes one node and its imediate edges for building the DAG.
  void buildNode(PPBLBlockNodeMap& inDag, std::stack<PPBallLarusNode*>& dfsStack);

  // Process an edge in the CFG for DAG building.
  void buildEdge(PPBLBlockNodeMap& inDag, std::stack<PPBallLarusNode*>& dfsStack,
                 PPBallLarusNode* currentNode, llvm::BasicBlock* succBB,
                 unsigned duplicateNumber);

  // The weight on each edge is the increment required along any path that
  // contains that edge.
  void calculatePathNumbersFrom(PPBallLarusNode* node);

  // Adds a backedge with its phony edges.  Updates the DAG state.
  void addBackedge(PPBallLarusNode* source, PPBallLarusNode* target,
                   unsigned duplicateCount);
};
} // end csi_inst namespace

#endif
