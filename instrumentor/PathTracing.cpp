//===------------------------- PathTracing.cpp ----------------------------===//
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
// with the LLVM Compiler Infrastructure, and was originally licensed under the
// University of Illinois Open Source License.  See UI_LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "path-tracing"

#include "BBCoverage.h"
#include "PathTracing.h"
#include "PrepareCSI.h"
#include "Utils.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
  #pragma GCC diagnostic pop

#include "Versions.h"
#include "llvm_proxy/DIBuilder.h"
#include "llvm_proxy/Module.h"
#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IntrinsicInst.h"

#include <climits>
#include <iostream>
#include <list>
#include <set>

using namespace csi_inst;
using namespace llvm;
using namespace std;

unsigned long HASH_THRESHHOLD;
unsigned int PATHS_SIZE;

// a special argument parser for unsigned longs
class ULongParser : public cl::parser<unsigned long> {
public:
#if LLVM_VERSION >= 30700
  using parser<unsigned long>::parser;
#endif

  // parse - Return true on error.
  bool parse(cl::Option &O, const StringRef ArgName, const StringRef &Arg,
             unsigned long &Val){
    (void)ArgName;
    if(Arg.getAsInteger(0, Val))
      return O.error("'" + Arg + "' value invalid for ulong argument!");
    return false;
  }
  
  // getValueName - Say what type we are expecting
  const char* getValueName() const{
    return "ulong";
  }
};

static cl::opt<bool> SilentInternal("pt-silent", cl::desc("Silence internal "
                                    "warnings.  Will still print errors which "
                                    "cause PT to fail."));

static cl::opt<bool> NoArrayWrites("pt-no-array-writes", cl::Hidden,
        cl::desc("Don't instrument loads and stores to the path array. "
                 "DEBUG ONLY"));

static cl::opt<unsigned long, false, ULongParser> HashSize("pt-hash-size",
                                  cl::desc("Set the maximum acyclic path count "
                                  "to instrument per function. "
                                  "Default: ULONG_MAX / 2 - 1"),
                                  cl::value_desc("hash_size"));

static cl::opt<int> ArraySize("pt-path-array-size", cl::desc("Set the size "
                                  "of the paths array for instrumented "
                                  "functions.  Default: 10"),
                                  cl::value_desc("path_array_size"));

static cl::opt<string> TrackerFile("pt-info-file", cl::desc("The path to "
                                   "the increment-line-number output file."),
                                   cl::value_desc("file_path"));

// Register path tracing as a pass
char PathTracing::ID = 0;
static RegisterPass<PathTracing> X("pt-inst",
                "Insert instrumentation for Ball-Larus tracing",
                false, false);

// Creates a new BLInstrumentationNode from a BasicBlock.
BLInstrumentationNode::BLInstrumentationNode(BasicBlock* BB) :
  PPBallLarusNode(BB),
  _startingPathNumber(NULL), _endingPathNumber(NULL), _pathPHI(NULL){
  static unsigned nextID = 0;
  _blockId = nextID++;
}

// Constructor for BLInstrumentationEdge.
BLInstrumentationEdge::BLInstrumentationEdge(BLInstrumentationNode* source,
                                             BLInstrumentationNode* target)
  : PPBallLarusEdge(source, target, 0),
    _increment(0), _isInSpanningTree(false), _isInitialization(false),
    _isCounterIncrement(false), _hasInstrumentation(false) {}

// Sets the target node of this edge.  Required to split edges.
void BLInstrumentationEdge::setTarget(PPBallLarusNode* node) {
  _target = node;
}

// Returns whether this edge is in the spanning tree.
bool BLInstrumentationEdge::isInSpanningTree() const {
  return(_isInSpanningTree);
}

// Sets whether this edge is in the spanning tree.
void BLInstrumentationEdge::setIsInSpanningTree(bool isInSpanningTree) {
  _isInSpanningTree = isInSpanningTree;
}

// Returns whether this edge will be instrumented with a path number
// initialization.
bool BLInstrumentationEdge::isInitialization() const {
  return(_isInitialization);
}

// Sets whether this edge will be instrumented with a path number
// initialization.
void BLInstrumentationEdge::setIsInitialization(bool isInitialization) {
  _isInitialization = isInitialization;
}

// Returns whether this edge will be instrumented with a path counter
// increment.  Notice this is incrementing the path counter
// corresponding to the path number register.  The path number
// increment is determined by getIncrement().
bool BLInstrumentationEdge::isCounterIncrement() const {
  return(_isCounterIncrement);
}

// Sets whether this edge will be instrumented with a path counter
// increment.
void BLInstrumentationEdge::setIsCounterIncrement(bool isCounterIncrement) {
  _isCounterIncrement = isCounterIncrement;
}

// Gets the path number increment that this edge will be instrumented
// with.  This is distinct from the path counter increment and the
// weight.  The counter increment is counts the number of executions of
// some path, whereas the path number keeps track of which path number
// the program is on.
long BLInstrumentationEdge::getIncrement() const {
  return(_increment);
}

// Set whether this edge will be instrumented with a path number
// increment.
void BLInstrumentationEdge::setIncrement(long increment) {
  if(increment < 0){
    DEBUG(dbgs() << "WARNING: we are setting a negative increment.  This is "
                 << "abnormal for an instrumented function.\n");
  }
  _increment = increment;
}

// True iff the edge has already been instrumented.
bool BLInstrumentationEdge::hasInstrumentation() {
  return(_hasInstrumentation);
}

// Set whether this edge has been instrumented.
void BLInstrumentationEdge::setHasInstrumentation(bool hasInstrumentation) {
  _hasInstrumentation = hasInstrumentation;
}

// Returns the successor number of this edge in the source.
unsigned BLInstrumentationEdge::getSuccessorNumber() {
  PPBallLarusNode* sourceNode = getSource();
  PPBallLarusNode* targetNode = getTarget();
  BasicBlock* source = sourceNode->getBlock();
  BasicBlock* target = targetNode->getBlock();

  if(source == NULL || target == NULL)
    return(0);

  TerminatorInst* terminator = source->getTerminator();

        unsigned i;
  for(i=0; i < terminator->getNumSuccessors(); i++) {
    if(terminator->getSuccessor(i) == target)
      break;
  }

  return(i);
}

// BLInstrumentationDag constructor initializes a DAG for the given Function.
BLInstrumentationDag::BLInstrumentationDag(Function &F) : PPBallLarusDag(F),
                                               _curIndex(0),
                                               _counterSize(0),
                                               _counterArray(0),
                                               _errorNegativeIncrements(false) {
}

// Returns the Exit->Root edge. This edge is required for creating
// directed cycles in the algorithm for moving instrumentation off of
// the spanning tree
PPBallLarusEdge* BLInstrumentationDag::getExitRootEdge() {
  PPBLEdgeIterator erEdge = getExit()->succBegin();
  return(*erEdge);
}

PPBLEdgeVector BLInstrumentationDag::getCallPhonyEdges () {
  PPBLEdgeVector callEdges;

  for( PPBLEdgeIterator edge = _edges.begin(), end = _edges.end();
       edge != end; edge++ ) {
    if( (*edge)->getType() == PPBallLarusEdge::CALLEDGE_PHONY )
      callEdges.push_back(*edge);
  }

  return callEdges;
}

// Gets the path counter array
Value* BLInstrumentationDag::getCounterArray() {
  return _counterArray;
}

// Get the current counter index
Value* BLInstrumentationDag::getCurIndex() {
  return _curIndex;
}

// Get the circular array size
int BLInstrumentationDag::getCounterSize() {
  return _counterSize;
}

// Set the circular array
void BLInstrumentationDag::setCounterArray(Value* c) {
  _counterArray = c;
}

// Set the current counter index
void BLInstrumentationDag::setCurIndex(Value* c) {
  _curIndex = c;
}

// Set the circular array size
void BLInstrumentationDag::setCounterSize(int s) {
  _counterSize = s;
}

// Gets the path tracker
Value* PathTracing::getPathTracker() {
  return _pathTracker;
}

// Set the path tracker
void PathTracing::setPathTracker(Value* c) {
  _pathTracker = c;
}

// Calculates the increment for the chords, thereby removing
// instrumentation from the spanning tree edges. Implementation is based on
// the algorithm in Figure 4 of [Ball94]
void BLInstrumentationDag::calculateChordIncrements() {
  calculateChordIncrementsDfs(0, getRoot(), NULL);

  BLInstrumentationEdge* chord;
  for(PPBLEdgeIterator chordEdge = _chordEdges.begin(),
      end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    chord = (BLInstrumentationEdge*) *chordEdge;
    long increment = chord->getIncrement() + chord->getWeight();
    if(increment < 0)
      this->_errorNegativeIncrements = true;
    chord->setIncrement(increment);
  }
}

// Updates the state when an edge has been split
void BLInstrumentationDag::splitUpdate(BLInstrumentationEdge* formerEdge,
                                       BasicBlock* newBlock) {
  PPBallLarusNode* oldTarget = formerEdge->getTarget();
  PPBallLarusNode* newNode = addNode(newBlock);
  formerEdge->setTarget(newNode);
  newNode->addPredEdge(formerEdge);

  oldTarget->removePredEdge(formerEdge);
  PPBallLarusEdge* newEdge = addEdge(newNode, oldTarget,0);

  if( formerEdge->getType() == PPBallLarusEdge::BACKEDGE ||
                        formerEdge->getType() == PPBallLarusEdge::SPLITEDGE) {
                newEdge->setType(formerEdge->getType());
    newEdge->setPhonyRoot(formerEdge->getPhonyRoot());
    newEdge->setPhonyExit(formerEdge->getPhonyExit());
    formerEdge->setType(PPBallLarusEdge::NORMAL);
                formerEdge->setPhonyRoot(NULL);
    formerEdge->setPhonyExit(NULL);
  }
}

// Calculates a spanning tree of the DAG ignoring cycles.  Whichever
// edges are in the spanning tree will not be instrumented, but this
// implementation does not try to minimize the instrumentation overhead
// by trying to find hot edges.
void BLInstrumentationDag::calculateSpanningTree() {
  stack<PPBallLarusNode*> dfsStack;

  for(PPBLNodeIterator nodeIt = _nodes.begin(), end = _nodes.end();
      nodeIt != end; nodeIt++) {
    (*nodeIt)->setColor(PPBallLarusNode::WHITE);
  }

  dfsStack.push(getRoot());
  while(!dfsStack.empty()) {
    PPBallLarusNode* node = dfsStack.top();
    dfsStack.pop();

    if(node->getColor() == PPBallLarusNode::WHITE)
      continue;

    PPBallLarusNode* nextNode;
    bool forward = true;
    PPBLEdgeIterator succEnd = node->succEnd();

    node->setColor(PPBallLarusNode::WHITE);
    // first iterate over successors then predecessors
    for(PPBLEdgeIterator edge = node->succBegin(), predEnd = node->predEnd();
        edge != predEnd; edge++) {
      if(edge == succEnd) {
        edge = node->predBegin();
        forward = false;
      }

      // Ignore split edges
      if ((*edge)->getType() == PPBallLarusEdge::SPLITEDGE)
        continue;

      nextNode = forward? (*edge)->getTarget(): (*edge)->getSource();
      if(nextNode->getColor() != PPBallLarusNode::WHITE) {
        nextNode->setColor(PPBallLarusNode::WHITE);
        makeEdgeSpanning((BLInstrumentationEdge*)(*edge));
      }
    }
  }

  for(PPBLEdgeIterator edge = _edges.begin(), end = _edges.end();
      edge != end; edge++) {
    BLInstrumentationEdge* instEdge = (BLInstrumentationEdge*) (*edge);
      // safe since createEdge is overriden
    if(!instEdge->isInSpanningTree() && (*edge)->getType()
        != PPBallLarusEdge::SPLITEDGE)
      _chordEdges.push_back(instEdge);
  }
}

// Pushes initialization further down in order to group the first
// increment and initialization.
void BLInstrumentationDag::pushInitialization() {
  BLInstrumentationEdge* exitRootEdge =
                (BLInstrumentationEdge*) getExitRootEdge();
  exitRootEdge->setIsInitialization(true);
  pushInitializationFromEdge(exitRootEdge);
}

// Pushes the path counter increments up in order to group the last path
// number increment.
void BLInstrumentationDag::pushCounters() {
  BLInstrumentationEdge* exitRootEdge =
    (BLInstrumentationEdge*) getExitRootEdge();
  exitRootEdge->setIsCounterIncrement(true);
  pushCountersFromEdge(exitRootEdge);
}

// Removes phony edges from the successor list of the source, and the
// predecessor list of the target.
void BLInstrumentationDag::unlinkPhony() {
  PPBallLarusEdge* edge;

  for(PPBLEdgeIterator next = _edges.begin(),
      end = _edges.end(); next != end; next++) {
    edge = (*next);

    if( edge->getType() == PPBallLarusEdge::BACKEDGE_PHONY ||
        edge->getType() == PPBallLarusEdge::SPLITEDGE_PHONY ||
        edge->getType() == PPBallLarusEdge::CALLEDGE_PHONY ) {
      unlinkEdge(edge);
    }
  }
}

// Allows subclasses to determine which type of Node is created.
// Override this method to produce subclasses of PPBallLarusNode if
// necessary. The destructor of PPBallLarusDag will call free on each pointer
// created.
PPBallLarusNode* BLInstrumentationDag::createNode(BasicBlock* BB) {
  return( new BLInstrumentationNode(BB) );
}

// Allows subclasses to determine which type of Edge is created.
// Override this method to produce subclasses of PPBallLarusEdge if
// necessary. The destructor of PPBallLarusDag will call free on each pointer
// created.
PPBallLarusEdge* BLInstrumentationDag::createEdge(PPBallLarusNode* source,
                                                PPBallLarusNode* target, unsigned edgeNumber) {
  (void)edgeNumber; // suppress warning
  // One can cast from PPBallLarusNode to BLInstrumentationNode since createNode
  // is overriden to produce BLInstrumentationNode.
  return( new BLInstrumentationEdge((BLInstrumentationNode*)source,
                                    (BLInstrumentationNode*)target) );
}

// Sets the Value corresponding to the pathNumber register, constant,
// or phinode.  Used by the instrumentation code to remember path
// number Values.
Value* BLInstrumentationNode::getStartingPathNumber(){
  return(_startingPathNumber);
}

// Sets the Value of the pathNumber.  Used by the instrumentation code.
void BLInstrumentationNode::setStartingPathNumber(Value* pathNumber) {
  DEBUG(dbgs() << "  SPN-" << getName() << " <-- " << (pathNumber ?
                                                       pathNumber->getName() :
                                                       "unused") << '\n');
  _startingPathNumber = pathNumber;
}

Value* BLInstrumentationNode::getEndingPathNumber(){
  return(_endingPathNumber);
}

void BLInstrumentationNode::setEndingPathNumber(Value* pathNumber) {
  DEBUG(dbgs() << "  EPN-" << getName() << " <-- "
               << (pathNumber ? pathNumber->getName() : "unused") << '\n');
  _endingPathNumber = pathNumber;
}

// Get the PHINode Instruction for this node.  Used by instrumentation
// code.
PHINode* BLInstrumentationNode::getPathPHI() {
  return(_pathPHI);
}

// Set the PHINode Instruction for this node.  Used by instrumentation
// code.
void BLInstrumentationNode::setPathPHI(PHINode* pathPHI) {
  _pathPHI = pathPHI;
}

// Get the unique ID of the node
unsigned int BLInstrumentationNode::getNodeId() {
  return(_blockId);
}

// Removes the edge from the appropriate predecessor and successor
// lists.
void BLInstrumentationDag::unlinkEdge(PPBallLarusEdge* edge) {
  if(edge == getExitRootEdge())
    DEBUG(dbgs() << " Removing exit->root edge\n");

  edge->getSource()->removeSuccEdge(edge);
  edge->getTarget()->removePredEdge(edge);
}

// Makes an edge part of the spanning tree.
void BLInstrumentationDag::makeEdgeSpanning(BLInstrumentationEdge* edge) {
  edge->setIsInSpanningTree(true);
  _treeEdges.push_back(edge);
}

// Pushes initialization and calls itself recursively.
void BLInstrumentationDag::pushInitializationFromEdge(
  BLInstrumentationEdge* edge) {
  PPBallLarusNode* target;

  target = edge->getTarget();
  if( target->getNumberPredEdges() > 1 || target == getExit() ) {
    return;
  } else {
    for(PPBLEdgeIterator next = target->succBegin(),
          end = target->succEnd(); next != end; next++) {
      BLInstrumentationEdge* intoEdge = (BLInstrumentationEdge*) *next;

      // Skip split edges
      if (intoEdge->getType() == PPBallLarusEdge::SPLITEDGE)
        continue;

      long increment = intoEdge->getIncrement() + edge->getIncrement();
      if(increment < 0)
        this->_errorNegativeIncrements = true;
      intoEdge->setIncrement(increment);
      intoEdge->setIsInitialization(true);
      pushInitializationFromEdge(intoEdge);
    }

    edge->setIncrement(0);
    edge->setIsInitialization(false);
  }
}

// Pushes path counter increments up recursively.
void BLInstrumentationDag::pushCountersFromEdge(BLInstrumentationEdge* edge) {
  PPBallLarusNode* source;

  source = edge->getSource();
  if(source->getNumberSuccEdges() > 1 || source == getRoot()
     || edge->isInitialization()) {
    return;
  } else {
    for(PPBLEdgeIterator previous = source->predBegin(),
          end = source->predEnd(); previous != end; previous++) {
      BLInstrumentationEdge* fromEdge = (BLInstrumentationEdge*) *previous;

      // Skip split edges
      if (fromEdge->getType() == PPBallLarusEdge::SPLITEDGE)
        continue;

      long increment = fromEdge->getIncrement() + edge->getIncrement();
      if(increment < 0)
        this->_errorNegativeIncrements = true;
      fromEdge->setIncrement(increment);
      fromEdge->setIsCounterIncrement(true);
      pushCountersFromEdge(fromEdge);
    }

    edge->setIncrement(0);
    edge->setIsCounterIncrement(false);
  }
}

// Depth first algorithm for determining the chord increments.
void BLInstrumentationDag::calculateChordIncrementsDfs(long weight,
                                                       PPBallLarusNode* v, PPBallLarusEdge* e) {
  BLInstrumentationEdge* f;

  for(PPBLEdgeIterator treeEdge = _treeEdges.begin(),
        end = _treeEdges.end(); treeEdge != end; treeEdge++) {
    f = (BLInstrumentationEdge*) *treeEdge;
    if(e != f && v == f->getTarget()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getSource(), f);
    }
    if(e != f && v == f->getSource()) {
      calculateChordIncrementsDfs(
        calculateChordIncrementsDir(e,f)*(weight) +
        f->getWeight(), f->getTarget(), f);
    }
  }

  for(PPBLEdgeIterator chordEdge = _chordEdges.begin(),
        end = _chordEdges.end(); chordEdge != end; chordEdge++) {
    f = (BLInstrumentationEdge*) *chordEdge;
    if(v == f->getSource() || v == f->getTarget()) {
      long increment = f->getIncrement() +
                       calculateChordIncrementsDir(e,f)*weight;
      if(increment < 0)
        this->_errorNegativeIncrements = true;
      f->setIncrement(increment);
    }
  }
}

// Determines the relative direction of two edges.
int BLInstrumentationDag::calculateChordIncrementsDir(PPBallLarusEdge* e,
                                                      PPBallLarusEdge* f) {
  if( e == NULL)
    return(1);
  else if(e->getSource() == f->getTarget()
          || e->getTarget() == f->getSource())
    return(1);

  return(-1);
}

bool BLInstrumentationDag::error_negativeIncrements(){
  return this->_errorNegativeIncrements;
}

// Creates an increment constant representing incr.
ConstantInt* PathTracing::createIncrementConstant(long incr,
                                                   int bitsize) {
  return(ConstantInt::get(IntegerType::get(*Context, bitsize), incr));
}

// Creates an increment constant representing the value in
// edge->getIncrement().
ConstantInt* PathTracing::createIncrementConstant(
  BLInstrumentationEdge* edge) {
  return(createIncrementConstant(edge->getIncrement(), 64));
}

// Creates a counter increment in the given node.  The Value* in node is
// taken as the index into an array or hash table.  The hash table access
// is a call to the runtime.
void PathTracing::insertCounterIncrement(Value* incValue,
                                          BasicBlock::iterator insertPoint,
                                          BLInstrumentationDag* dag) {
  // first, find out the current location
  LoadInst* curLoc = new LoadInst(dag->getCurIndex(), "curIdx", &*insertPoint);
  
  // Counter increment for array
  if( dag->getNumberOfPaths() <= HASH_THRESHHOLD ) {
    // Get pointer to the array location
    Value * const gepIndices[] = {
      Constant::getNullValue(Type::getInt64Ty(*Context)),
      curLoc,
    };

    GetElementPtrInst* pcPointer =
      GetElementPtrInst::CreateInBounds(dag->getCounterArray(), gepIndices,
                                        "arrLoc", &*insertPoint);

    // Store back in to the array
    new StoreInst(incValue, pcPointer, true, &*insertPoint);
    
    // update the next circular buffer location
    Type* tInt = Type::getInt64Ty(*Context);
    BinaryOperator* addLoc = BinaryOperator::Create(Instruction::Add,
                                                    curLoc,
                                                    ConstantInt::get(tInt, 1),
                                                    "addLoc", &*insertPoint);
    Instruction* atEnd = new ICmpInst(&*insertPoint, CmpInst::ICMP_EQ, curLoc,
                               ConstantInt::get(tInt, dag->getCounterSize()-1),
                               "atEnd");
    Instruction* nextLoc = SelectInst::Create(&*atEnd, ConstantInt::get(tInt, 0),
                                              addLoc, "nextLoc", &*insertPoint);
    
    new StoreInst(nextLoc, dag->getCurIndex(), true, &*insertPoint);
    new StoreInst(ConstantInt::get(tInt, 0), this->getPathTracker(), true,
                  &*insertPoint);
  }
  else {
    // Counter increment for hash would have gone here (should actually be
    // unreachable now)
    errs() << "ERROR: instrumentation continued in function with over-large"
           << " path count.  This is a tool error. Results will be wrong!\n";
    exit(1);
  }
}

static BasicBlock::iterator getTerminator(BLInstrumentationNode &node)
{
  return node.getBlock()->getTerminator()
#if LLVM_VERSION >= 30800
    ->getIterator()
#endif
    ;
}


void PathTracing::insertInstrumentationStartingAt(BLInstrumentationEdge* edge,
                                                   BLInstrumentationDag* dag) {
  // Mark the edge as instrumented
  edge->setHasInstrumentation(true);

  // create a new node for this edge's instrumentation
  splitCritical(edge, dag);

  BLInstrumentationNode* sourceNode = (BLInstrumentationNode*)edge->getSource();
  BLInstrumentationNode* targetNode = (BLInstrumentationNode*)edge->getTarget();
  BLInstrumentationNode* instrumentNode;

  bool atBeginning = false;

  // Source node has only 1 successor so any information can be simply
  // inserted in to it without splitting
  if( sourceNode->getBlock() && sourceNode->getNumberSuccEdges() <= 1) {
    DEBUG(dbgs() << "  Potential instructions to be placed in: "
          << sourceNode->getName() << " (at end)\n");
    instrumentNode = sourceNode;
  }

  // The target node only has one predecessor, so we can safely insert edge
  // instrumentation into it. If there was splitting, it must have been
  // successful.
  else if( targetNode->getNumberPredEdges() == 1 ) {
    DEBUG(dbgs() << "  Potential instructions to be placed in: "
          << targetNode->getName() << " (at beginning)\n");
    instrumentNode = targetNode;
    atBeginning = true;
  }

  // Somehow, splitting must have failed.
  else {
    errs() << "ERROR: Instrumenting could not split a critical edge.\n";
    exit(1);
  }

  // Insert instrumentation if this is a back or split edge
  if( edge->getType() == PPBallLarusEdge::BACKEDGE ||
      edge->getType() == PPBallLarusEdge::SPLITEDGE ) {
    BLInstrumentationEdge* top =
      (BLInstrumentationEdge*) edge->getPhonyRoot();
    BLInstrumentationEdge* bottom =
      (BLInstrumentationEdge*) edge->getPhonyExit();

    assert( top->isInitialization() && " Top phony edge did not"
            " contain a path number initialization.");
    assert( bottom->isCounterIncrement() && " Bottom phony edge"
            " did not contain a path counter increment.");

    BasicBlock::iterator insertPoint = atBeginning ?
      instrumentNode->getBlock()->getFirstInsertionPt() :
      getTerminator(*instrumentNode);

    // add information from the bottom edge, if it exists
    if( bottom->getIncrement() ) {
      Instruction* oldValue = new LoadInst(this->getPathTracker(),
                                           "oldValBackSplit", &*insertPoint);
      Instruction* newValue = BinaryOperator::Create(Instruction::Add,
                                 oldValue, createIncrementConstant(bottom),
                                 "pathNumber", &*insertPoint);
      new StoreInst(newValue, this->getPathTracker(), true, &*insertPoint);
    }

    if(!NoArrayWrites){
      Instruction* curValue = new LoadInst(this->getPathTracker(),
                                           "curValBackSplit", &*insertPoint);
      insertCounterIncrement(curValue, insertPoint, dag);
    }
    
    new StoreInst(createIncrementConstant(top), this->getPathTracker(), true,
                  &*insertPoint);
    
    // Check for path counter increments: we would never expect the top edge to
    // also be a counter increment, but we'll handle it anyways
    if( top->isCounterIncrement() ) {
      DEBUG(dbgs() << "WARNING: a top counter increment encountered\n");
      if(!NoArrayWrites){
        insertCounterIncrement(createIncrementConstant(top),
                               getTerminator(*instrumentNode), dag);
      }
    }
  }

  // Insert instrumentation if this is a normal edge
  else {
    BasicBlock::iterator insertPoint = atBeginning ?
      instrumentNode->getBlock()->getFirstInsertionPt() :
      getTerminator(*instrumentNode);

    if(edge->isInitialization()){ // initialize path number
      new StoreInst(createIncrementConstant(edge), this->getPathTracker(), true,
                    &*insertPoint);
    }
    else if(edge->getIncrement()){// increment path number
      Instruction* oldValue = new LoadInst(this->getPathTracker(),
                                           "oldVal", &*insertPoint);
      Instruction* newValue = BinaryOperator::Create(Instruction::Add,
                                 oldValue, createIncrementConstant(edge),
                                 "pathNumber", &*insertPoint);
      new StoreInst(newValue, this->getPathTracker(), true, &*insertPoint);
    }

    // Check for path counter increments (function exit)
    if(edge->isCounterIncrement()){
      if(!NoArrayWrites){
        Instruction* curValue = new LoadInst(this->getPathTracker(),
                                             "curVal", &*insertPoint);
        insertCounterIncrement(curValue, insertPoint, dag);
      }
    }
  }

  // Add all the successors
  for( PPBLEdgeIterator next = targetNode->succBegin(),
         end = targetNode->succEnd(); next != end; next++ ) {
    // So long as it is un-instrumented, add it to the list
    if( !((BLInstrumentationEdge*)(*next))->hasInstrumentation() )
      insertInstrumentationStartingAt((BLInstrumentationEdge*)*next,dag);
  }
}

void PathTracing::insertInstrumentation(BLInstrumentationDag& dag){

  BLInstrumentationEdge* exitRootEdge =
    (BLInstrumentationEdge*) dag.getExitRootEdge();
  insertInstrumentationStartingAt(exitRootEdge, &dag);
  
  PPBLEdgeVector callEdges = dag.getCallPhonyEdges();
  if(callEdges.begin() != callEdges.end()){
    errs() << "ERROR: phony edges were inserted for calls.  This is not "
           << "supported for path tracing.  Do not use flag "
           << "'-path-profile-early-termination'\n";
    exit(1);
  }
}

// If this edge is a critical edge, then inserts a node at this edge.
// This edge becomes the first edge, and a new PPBallLarusEdge is created.
// Returns true if the edge was split
bool PathTracing::splitCritical(BLInstrumentationEdge* edge,
                                 BLInstrumentationDag* dag) {
  unsigned succNum = edge->getSuccessorNumber();
  PPBallLarusNode* sourceNode = edge->getSource();
  PPBallLarusNode* targetNode = edge->getTarget();
  BasicBlock* sourceBlock = sourceNode->getBlock();
  BasicBlock* targetBlock = targetNode->getBlock();

  if(sourceBlock == NULL || targetBlock == NULL
     || sourceNode->getNumberSuccEdges() <= 1
     || targetNode->getNumberPredEdges() == 1 ) {
    return(false);
  }

  TerminatorInst* terminator = sourceBlock->getTerminator();

#if LLVM_VERSION < 30700
  BasicBlock * const newBlock = SplitCriticalEdge(terminator, succNum, this, false);
#else
  BasicBlock * const newBlock { SplitCriticalEdge(terminator, succNum) };
#endif  
  if (newBlock) {
    dag->splitUpdate(edge, newBlock);
    return(true);
  } else
    return(false);
}





// NOTE: could handle inlining specially here if desired.
static void writeBBLineNums(BasicBlock* bb,
                            BLInstrumentationDag* dag,
                            raw_ostream& stream = outs()){
  if(!bb){
    stream << "|NULL";
    return;
  }
  
  bool any = false;
  DebugLoc dbLoc;
  for(BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i){
    if(BranchInst* inst = dyn_cast<BranchInst>(i))
      if(inst->isUnconditional())
        continue;
    
    dbLoc = i->getDebugLoc();
    if (!isUnknown(dbLoc)) {
      stream << '|' << dbLoc.getLine(); // << ':' << dbLoc.getCol();
      any = true;
    }
    else if(LoadInst* inst = dyn_cast<LoadInst>(&*i)){
      if(inst->getPointerOperand() == dag->getCurIndex() &&
         inst->getName().find("curIdx") == 0){
        stream << "|-1";
        any = true;
      }
    }
  }
  
  // write NULL if there are no debug locations in the basic block
  if(!any)
    stream << "|NULL";
}


static void writeBBLineNums(BasicBlock* bb, BLInstrumentationDag* dag, ostream& stream){
  raw_os_ostream wrapped(stream);
  writeBBLineNums(bb, dag, wrapped);
}


void PathTracing::writeBBs(Function& F, BLInstrumentationDag* dag){
  BLInstrumentationEdge* root = (BLInstrumentationEdge*)dag->getExitRootEdge();
  list<BLInstrumentationEdge*> edgeWl;
  set<BLInstrumentationNode*> done;
  //edgeWl.push_back(root);
  
  // special handling here for exit node (mark as "EXIT")
  BLInstrumentationNode* exitNode = (BLInstrumentationNode*)root->getSource();
  trackerStream << exitNode->getNodeId() << "|EXIT" << '\n';
  if(exitNode->getBlock()){
    errs() << "ERROR: Exit node has associated basic block in function "
           << F.getName() << ".  This is a tool error.\n";
    exit(1);
  }
  done.insert(exitNode);
  
  // special handling here for entry node (mark as "ENTRY" but also write line#)
  BLInstrumentationNode* entryNode = (BLInstrumentationNode*)root->getTarget();
  trackerStream << entryNode->getNodeId() << "|ENTRY";
  writeBBLineNums(entryNode->getBlock(), dag, trackerStream);
  trackerStream << '\n';
  for(PPBLEdgeIterator i = entryNode->succBegin(), e = entryNode->succEnd(); i != e; ++i){
    edgeWl.push_back((BLInstrumentationEdge*)(*i));
  }
  done.insert(entryNode);
  
  while(!edgeWl.empty()){
    BLInstrumentationNode* current =
       (BLInstrumentationNode*)edgeWl.front()->getTarget();
    edgeWl.pop_front();
    if(done.count(current))
      continue;
    
    trackerStream << current->getNodeId();
    writeBBLineNums(current->getBlock(), dag, trackerStream);
    trackerStream << '\n';
    
    for(PPBLEdgeIterator i = current->succBegin(), e = current->succEnd(); i != e; ++i){
      edgeWl.push_back((BLInstrumentationEdge*)(*i));
    }
    
    done.insert(current);
  }
}

void PathTracing::writeTrackerInfo(Function& F, BLInstrumentationDag* dag){
  trackerStream << "#\n" << F.getName().str() << '\n';
  
  writeBBs(F, dag);
  trackerStream << "$\n";
  
  BLInstrumentationEdge* root = (BLInstrumentationEdge*)dag->getExitRootEdge();
  list<BLInstrumentationEdge*> edgeWl;
  set<BLInstrumentationEdge*> done;
  
  // handle the root node specially
  BLInstrumentationNode* rTarget = (BLInstrumentationNode*)root->getTarget();
  for(PPBLEdgeIterator i = rTarget->succBegin(), e = rTarget->succEnd(); i != e; ++i){
    edgeWl.push_back((BLInstrumentationEdge*)(*i));
  }
  done.insert(root);
  
  while(!edgeWl.empty()){
    BLInstrumentationEdge* current = edgeWl.front();
    edgeWl.pop_front();
    if(done.count(current))
      continue;
    
    BLInstrumentationNode* source = (BLInstrumentationNode*)current->getSource();
    BLInstrumentationNode* target = (BLInstrumentationNode*)current->getTarget();
    
    if(current->getType() == PPBallLarusEdge::BACKEDGE ||
       current->getType() == PPBallLarusEdge::SPLITEDGE){
      BLInstrumentationEdge* phony =
         (BLInstrumentationEdge*)current->getPhonyRoot();
      trackerStream << source->getNodeId() << "~>" << target->getNodeId()
                     << '|' << phony->getIncrement()
                     << '$' << phony->getWeight() << '\n';
    }
    else{
      trackerStream << source->getNodeId() << "->" << target->getNodeId()
                     << '|' << current->getIncrement()
                     << '$' << current->getWeight()
                     << '\n';
    }
    
    for(PPBLEdgeIterator i = target->succBegin(), e = target->succEnd(); i != e; ++i){
      edgeWl.push_back((BLInstrumentationEdge*)(*i));
    }
    
    done.insert(current);
  }
}


static AllocaInst *createAllocaInst(Type *type, const Twine &name, Instruction *insertBefore)
{
  return new AllocaInst(type,
#if LLVM_VERSION >= 50000
			0,
#endif
			name, insertBefore);
}


// Entry point of the function
bool PathTracing::runOnFunction(Function &F) {
  PrepareCSI& instData = getAnalysis<PrepareCSI>();
  if(F.isDeclaration() || !instData.hasInstrumentationType(F, "PT"))
    return false;
  DEBUG(dbgs() << "Function: " << F.getName() << '\n');
  
  // Build DAG from CFG
  BLInstrumentationDag dag = BLInstrumentationDag(F);
  dag.init();

  // give each path a unique integer value
  dag.calculatePathNumbers();

  // modify path increments to increase the efficiency
  // of instrumentation
  dag.calculateSpanningTree();
  dag.calculateChordIncrements();
  dag.pushInitialization();
  dag.pushCounters();
  dag.unlinkPhony();
  
  // We only support the information in an array for now
  if( dag.getNumberOfPaths() <= HASH_THRESHHOLD ) {
    if(dag.error_negativeIncrements()){
      errs() << "ERROR: Instrumentation is proceeding while DAG structure is "
             << "in error and contains a negative increment for function "
             << F.getName() << ".  This is a tool error.\n";
      exit(1);
    }
    else if(dag.error_edgeOverflow()){
      errs() << "ERROR: Instrumentation is proceeding while DAG structure is "
             << "in error due to an edge weight overflow for function "
             << F.getName() << ".  This is a tool error.\n";
      exit(2);
    }
    
    Type* tInt = Type::getInt64Ty(*Context);
    Type* tArr = ArrayType::get(tInt, PATHS_SIZE);

    // declare the path index and array
    Instruction* entryInst = F.getEntryBlock().getFirstNonPHI();
    AllocaInst* arrInst = createAllocaInst(tArr, "__PT_pathArr", entryInst);
    AllocaInst* idxInst = createAllocaInst(tInt, "__PT_arrIndex", entryInst);
    new StoreInst(ConstantInt::get(tInt, 0), idxInst, true, entryInst);
    Instruction* trackInst = createAllocaInst(tInt, "__PT_curPath", entryInst);
    new StoreInst(ConstantInt::get(tInt, 0), trackInst, true, entryInst);
    
    // Store the setinal value (-1) into pathArr[end]
    Value * const gepIndices[] = {
      Constant::getNullValue(Type::getInt64Ty(*Context)),
      ConstantInt::get(tInt, PATHS_SIZE-1),
    };
    GetElementPtrInst* arrLast = GetElementPtrInst::CreateInBounds(arrInst,
                                                                   gepIndices,
                                                                   "arrLast",
                                                                   entryInst);
    new StoreInst(ConstantInt::get(tInt, -1), arrLast, true, entryInst);
    
    dag.setCounterArray(arrInst);
    dag.setCurIndex(idxInst);
    dag.setCounterSize(PATHS_SIZE);
    this->setPathTracker(trackInst);
    
    // create debug info for new variables
    DIBuilder Builder(*F.getParent());
#if LLVM_VERSION < 30700
    const DIType intType = Builder.createBasicType("__pt_int", 64, 64, dwarf::DW_ATE_signed);
    const DIType arrType = createArrayType(Builder, PATHS_SIZE, intType);
#else
    DIType * const intType { createBasicType(Builder, "__pt_int", 64, dwarf::DW_ATE_signed) };
    DIType * const arrType { createArrayType(Builder, PATHS_SIZE, intType) };
#endif
    
    // get the debug location of any instruction in this basic block--this will
    // use the same info.  If there is none, technically we should build it, but
    // that's a huge pain (if it's possible) so I just give up right now
    const DebugLoc dbLoc = findEarlyDebugLoc(F, SilentInternal);
    if (!isUnknown(dbLoc)) {
#if LLVM_VERSION < 30700
      typedef DIVariable Info;
      const DIDescriptor scope(dbLoc.getScope(*Context));
      const DIFile file(dbLoc.getScope(*Context));
#else
      typedef DILocalVariable *Info;
      DIScope * const scope { dbLoc->getScope() };
      DIFile * const file { scope->getFile() };
#endif
      const Info arrDI = createAutoVariable(
                            Builder,
                            scope,
                            "__PT_counter_arr",
                            file, 0, arrType, true);
      insertDeclare(Builder, arrInst, arrDI, dbLoc, entryInst);
      const Info idxDI = createAutoVariable(
                            Builder,
                            scope,
                            "__PT_counter_idx",
                            file, 0, intType, true);
      insertDeclare(Builder, idxInst, idxDI, dbLoc, entryInst);
      const Info trackDI = createAutoVariable(
                             Builder,
                             scope,
                             "__PT_current_path",
                             file, 0, intType,
                             true);
      insertDeclare(Builder, trackInst, trackDI, dbLoc, entryInst);
    }
    
    // do the instrumentation and write out the path info to the .info file
    insertInstrumentation(dag);
    
    writeTrackerInfo(F, &dag);
  }
  else if(!SilentInternal){
    errs() << "WARNING: instrumentation not done for function "
           << F.getName() << " due to large path count.  Path info "
           << "will be missing!\n";
    return false;
  }
  
  return true;
}

bool PathTracing::runOnModule(Module& M){
  static bool runBefore = false;
  if(runBefore)
    return(false);
  runBefore = true;
  
  if(TrackerFile.empty())
    report_fatal_error("PT cannot continue: -pt-tracker-file [file] is required", false);
  trackerStream.open(TrackerFile.c_str(), ios::out | ios::trunc);
  if(!trackerStream.is_open())
    report_fatal_error("unable to open pt-file location: " + TrackerFile);
  DEBUG(dbgs() << "Output stream opened to " << TrackerFile << '\n');
  
  this->Context = &M.getContext();
  
  if(ArraySize > 0)
    PATHS_SIZE = ArraySize;
  else
    PATHS_SIZE = 10;
  if(HashSize > 0)
    HASH_THRESHHOLD = HashSize;
  else
    HASH_THRESHHOLD = ULONG_MAX / 2 - 1;
  
  bool changed = false;
  for(Module::iterator i = M.begin(), e = M.end(); i != e; ++i){
    changed |= runOnFunction(*i);
  }
  
  trackerStream.close();
  return changed;
}

// Output the bitcode if we want to observe instrumentation changess
#define PRINT_MODULE dbgs() <<                               \
  "\n\n============= MODULE BEGIN ===============\n" << M << \
  "\n============== MODULE END ================\n"

void PathTracing::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<PrepareCSI>();
  AU.addPreserved<PrepareCSI>();

  // enforce that PT runs after BBC
  // this is horribly ugly but now necessary for two reasons:
  // (1) optimization uses the dominator tree, and PT modifies the CFG
  // (2) since PT modifies the CFG, the number of BBs changes, and thus so
  //     does the BBC array size
  // it gets worse: this is the reason that we have to now prevent a pass from
  //                running twice (which always could happen, but never did)
  // NOTE: if any other requirements are added (e.g. that CC also runs before
  //       PT) a crash ensues.  Well played LLVM; well played.
  AU.addRequired<BBCoverage>();
  AU.addPreserved<BBCoverage>();
}
