//===-------------------------- PathTracing.h -----------------------------===//
//
// This pass instruments functions to perform path tracing.  The method is
// based on Ball-Larus path profiling, originally published in [1].
//
// [1]
//  T. Ball and J. R. Larus. "Efficient Path Profiling."
//  International Symposium on Microarchitecture, pages 46-57, 1996.
//  http://portal.acm.org/citation.cfm?id=243857
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
// This file is derived from the file PathProfiling.cpp, originally distributed
// with the LLVM Compiler Infrastructure, originally licensed under the
// University of Illinois Open Source License.  See UI_LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//
#ifndef CSI_PATH_TRACING_H
#define CSI_PATH_TRACING_H

#include "PassName.h"
#include "PathNumbering.h"

#include <llvm/Pass.h>

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Instructions.h"

#include <fstream>

namespace csi_inst {
class BLInstrumentationNode;
class BLInstrumentationEdge;
class BLInstrumentationDag;

// ---------------------------------------------------------------------------
// BLInstrumentationNode extends PPBallLarusNode with member used by the
// instrumentation algortihms.
// ---------------------------------------------------------------------------
class BLInstrumentationNode : public PPBallLarusNode {
public:
  // Creates a new BLInstrumentationNode from a BasicBlock.
  BLInstrumentationNode(llvm::BasicBlock* BB);

  // Get/sets the Value corresponding to the pathNumber register,
  // constant or phinode.  Used by the instrumentation code to remember
  // path number Values.
  llvm::Value* getStartingPathNumber();
  void setStartingPathNumber(llvm::Value* pathNumber);

  llvm::Value* getEndingPathNumber();
  void setEndingPathNumber(llvm::Value* pathNumber);

  // Get/set the PHINode Instruction for this node.
  llvm::PHINode* getPathPHI();
  void setPathPHI(llvm::PHINode* pathPHI);
  
  // Node ID access is required to help with writing out block data
  unsigned int getNodeId();

private:

  llvm::Value* _startingPathNumber; // The Value for the current pathNumber.
  llvm::Value* _endingPathNumber; // The Value for the current pathNumber.
  llvm::PHINode* _pathPHI; // The PHINode for current pathNumber.
  
  unsigned int _blockId; // The unique ID for the node
};

// --------------------------------------------------------------------------
// BLInstrumentationEdge extends PPBallLarusEdge with data about the
// instrumentation that will end up on each edge.
// --------------------------------------------------------------------------
class BLInstrumentationEdge : public PPBallLarusEdge {
public:
  BLInstrumentationEdge(BLInstrumentationNode* source,
                        BLInstrumentationNode* target);

  // Sets the target node of this edge.  Required to split edges.
  void setTarget(PPBallLarusNode* node);

  // Get/set whether edge is in the spanning tree.
  bool isInSpanningTree() const;
  void setIsInSpanningTree(bool isInSpanningTree);

  // Get/ set whether this edge will be instrumented with a path number
  // initialization.
  bool isInitialization() const;
  void setIsInitialization(bool isInitialization);

  // Get/set whether this edge will be instrumented with a path counter
  // increment.  Notice this is incrementing the path counter
  // corresponding to the path number register.  The path number
  // increment is determined by getIncrement().
  bool isCounterIncrement() const;
  void setIsCounterIncrement(bool isCounterIncrement);

  // Get/set the path number increment that this edge will be instrumented
  // with.  This is distinct from the path counter increment and the
  // weight.  The counter increment counts the number of executions of
  // some path, whereas the path number keeps track of which path number
  // the program is on.
  long getIncrement() const;
  void setIncrement(long increment);

  // Get/set whether the edge has been instrumented.
  bool hasInstrumentation();
  void setHasInstrumentation(bool hasInstrumentation);

  // Returns the successor number of this edge in the source.
  unsigned getSuccessorNumber();

private:
  // The increment that the code will be instrumented with.
  long _increment;

  // Whether this edge is in the spanning tree.
  bool _isInSpanningTree;

  // Whether this edge is an initialiation of the path number.
  bool _isInitialization;

  // Whether this edge is a path counter increment.
  bool _isCounterIncrement;

  // Whether this edge has been instrumented.
  bool _hasInstrumentation;
};

// ---------------------------------------------------------------------------
// BLInstrumentationDag extends PPBallLarusDag with algorithms that
// determine where instrumentation should be placed.
// ---------------------------------------------------------------------------
class BLInstrumentationDag : public PPBallLarusDag {
public:
  BLInstrumentationDag(llvm::Function &F);

  // Returns the Exit->Root edge. This edge is required for creating
  // directed cycles in the algorithm for moving instrumentation off of
  // the spanning tree
  PPBallLarusEdge* getExitRootEdge();

  // Returns an array of phony edges which mark those nodes
  // with function calls
  PPBLEdgeVector getCallPhonyEdges();

  // Gets/sets the path counter array
  llvm::Value* getCounterArray();
  llvm::Value* getCurIndex();
  int getCounterSize();
  void setCounterArray(llvm::Value* c);
  void setCurIndex(llvm::Value* c);
  void setCounterSize(int s);

  // Calculates the increments for the chords, thereby removing
  // instrumentation from the spanning tree edges. Implementation is based
  // on the algorithm in Figure 4 of [Ball94]
  void calculateChordIncrements();

  // Updates the state when an edge has been split
  void splitUpdate(BLInstrumentationEdge* formerEdge,
                   llvm::BasicBlock* newBlock);

  // Calculates a spanning tree of the DAG ignoring cycles.  Whichever
  // edges are in the spanning tree will not be instrumented, but this
  // implementation does not try to minimize the instrumentation overhead
  // by trying to find hot edges.
  void calculateSpanningTree();

  // Pushes initialization further down in order to group the first
  // increment and initialization.
  void pushInitialization();

  // Pushes the path counter increments up in order to group the last path
  // number increment.
  void pushCounters();

  // Removes phony edges from the successor list of the source, and the
  // predecessor list of the target.
  void unlinkPhony();
  
  // Whether or not the DAG is in error due to negative edge increments
  bool error_negativeIncrements();

protected:
  // BLInstrumentationDag creates BLInstrumentationNode objects in this
  // method overriding the creation of PPBallLarusNode objects.
  //
  // Allows subclasses to determine which type of Node is created.
  // Override this method to produce subclasses of PPBallLarusNode if
  // necessary.
  virtual PPBallLarusNode* createNode(llvm::BasicBlock* BB);

  // BLInstrumentationDag create BLInstrumentationEdges.
  //
  // Allows subclasses to determine which type of Edge is created.
  // Override this method to produce subclasses of PPBallLarusEdge if
  // necessary.  Parameters source and target will have been created by
  // createNode and can be cast to the subclass of PPBallLarusNode*
  // returned by createNode.
  virtual PPBallLarusEdge* createEdge(
    PPBallLarusNode* source, PPBallLarusNode* target, unsigned edgeNumber);

private:
  PPBLEdgeVector _treeEdges; // All edges in the spanning tree.
  PPBLEdgeVector _chordEdges; // All edges not in the spanning tree.
  llvm::Value* _curIndex;     // The current index into the counters array
  int _counterSize;     // The size of the circular counter array
  llvm::Value* _counterArray; // Array to store path counters
  
  bool _errorNegativeIncrements; // DAG in an error state with negative incs?
  
  // Removes the edge from the appropriate predecessor and successor lists.
  void unlinkEdge(PPBallLarusEdge* edge);

  // Makes an edge part of the spanning tree.
  void makeEdgeSpanning(BLInstrumentationEdge* edge);

  // Pushes initialization and calls itself recursively.
  void pushInitializationFromEdge(BLInstrumentationEdge* edge);

  // Pushes path counter increments up recursively.
  void pushCountersFromEdge(BLInstrumentationEdge* edge);

  // Depth first algorithm for determining the chord increments.f
  void calculateChordIncrementsDfs(
    long weight, PPBallLarusNode* v, PPBallLarusEdge* e);

  // Determines the relative direction of two edges.
  int calculateChordIncrementsDir(PPBallLarusEdge* e, PPBallLarusEdge* f);
};

// ---------------------------------------------------------------------------
// PathTracing is a module pass which instruments based on Ball/Larus
// path profiling
// ---------------------------------------------------------------------------
class PathTracing : public llvm::ModulePass {
private:
  // Current context for multi threading support.
  llvm::LLVMContext* Context;
  
  // Gets/sets the current path tracker and it's debug info
  llvm::Value* getPathTracker();
  void setPathTracker(llvm::Value* c);
  llvm::Value* _pathTracker; // The storage for the current path tracker
  
  std::ofstream trackerStream; // The output stream to the tracker file
                               // (managed by runOnFunction and written to as
                               // we go)

  // Analyzes and instruments the function for path tracing
  bool runOnFunction(llvm::Function &F);
  // Perform module-level tasks, open streams, and instrument each function
  bool runOnModule(llvm::Module &M);

  // Creates an increment constant representing incr.
  llvm::ConstantInt* createIncrementConstant(long incr, int bitsize);

  // Creates an increment constant representing the value in
  // edge->getIncrement().
  llvm::ConstantInt* createIncrementConstant(BLInstrumentationEdge* edge);

  // Creates a counter increment in the given node.  The Value* in node is
  // taken as the index into a hash table.
  void insertCounterIncrement(llvm::Value* incValue,
                              llvm::BasicBlock::iterator insertPoint,
                              BLInstrumentationDag* dag);

  // Inserts instrumentation for the given edge
  //
  // Pre: The edge's source node has pathNumber set if edge is non zero
  // path number increment.
  //
  // Post: Edge's target node has a pathNumber set to the path number Value
  // corresponding to the value of the path register after edge's
  // execution.
  void insertInstrumentationStartingAt(
    BLInstrumentationEdge* edge,
    BLInstrumentationDag* dag);
  
  // If this edge is a critical edge, then inserts a node at this edge.
  // This edge becomes the first edge, and a new PPBallLarusEdge is created.
  bool splitCritical(BLInstrumentationEdge* edge, BLInstrumentationDag* dag);

  // Inserts instrumentation according to the marked edges in dag.  Phony
  // edges must be unlinked from the DAG, but accessible from the
  // backedges.  Dag must have initializations, path number increments, and
  // counter increments present.
  //
  // Counter storage is created here.
  void insertInstrumentation( BLInstrumentationDag& dag);
  
  // Writes the tracker increments (and initial tracker values)
  void writeTrackerInfo(llvm::Function& F, BLInstrumentationDag* dag);
  
  // Writes out bb #s mapped to their source line numbers
  void writeBBs(llvm::Function& F, BLInstrumentationDag* dag);

public:
  static char ID; // Pass identification, replacement for typeid
  PathTracing() : ModulePass(ID) {}

  virtual PassName getPassName() const {
    return "Intraprocedural Path Tracing";
  }
  
  void getAnalysisUsage(llvm::AnalysisUsage &) const;
};
} // end csi_inst namespace

#endif
