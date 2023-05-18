//===-------------------------- BBCoverage.cpp ----------------------------===//
//
// This pass instruments basic blocks for interprocedural analysis by
// gathering both global and local coverage information.
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
#define DEBUG_TYPE "bb-coverage"

#include "BBCoverage.h"
#include "CoveragePassNames.h"
#include "CoverageOptimization.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IntrinsicInst.h"
#include "llvm_proxy/Module.h"

using namespace csi_inst;
using namespace llvm;
using namespace std;


const CoveragePassNames BBCoverage::names = {
  "bbc",
  "BBC",
  "basic block",
  "Basic Block",
};

BBCoverage::Options BBCoverage::options(
   names,
   "same as O2"
);

// Register basic block coverage as a pass
char BBCoverage::ID = 0;
static RegisterPass<BBCoverage> X("bb-coverage",
                "Insert basic block coverage instrumentation",
                false, false);


// No longer requires any additional LLVM passes
void BBCoverage::getAnalysisUsage(AnalysisUsage &AU) const {
  LocalCoveragePass::getAnalysisUsage(AU);
}


set<BasicBlock*> BBCoverage::getOptimizedInstrumentation(Function& F){
  CoverageOptimizationData& sgData = getAnalysis<CoverageOptimizationData>(F);
  
  set<BasicBlock*> result;
  switch (options.optimizationLevel)
    {
    case OptimizationOption::O1:
      // this is now equivalent to O2 (since the superblock stuff is no more,
      // and we only talk about possibly-incomplete executions)
    case OptimizationOption::O2:
      result = sgData.getOptimizedProbes(&F);
      break;
    case OptimizationOption::O3:
#if defined(USE_GAMS) || defined(USE_LEMON)
      result = sgData.getOptimizedProbes(&F, NULL, NULL, true);
      break;
#else
      report_fatal_error("csi build does not support optimization level 3. "
                         "csi must be built with GAMS or LEMON optimization "
                         "enabled");
#endif
    default:
      report_fatal_error("basic block optimization level reached invalid "
                         "point in function '" + F.getName() + "'");
    }

  DEBUG(dbgs() << "instrumenting: " << setBB_asstring(result) << '\n');
  return(result);
}

void BBCoverage::writeOneBB(BasicBlock* theBlock, unsigned int index,
                            bool isInstrumented){
  infoStream << (isInstrumented?"":"-") << index << '|'
             << (isInstrumented ? indexToLabel(index) : "");
  
  bool printedOne = false;
  for(BasicBlock::iterator i = theBlock->begin(), e = theBlock->end(); i != e; ++i){
    if(BranchInst* inst = dyn_cast<BranchInst>(i))
      if(inst->isUnconditional())
        continue;

    DebugLoc dbLoc = (&*i)->getDebugLoc();
    if (isUnknown(dbLoc))
      continue;
    
    infoStream << '|' << dbLoc.getLine();
    printedOne = true;
  }
  
  if(!printedOne)
    infoStream << "|NULL";
  
  infoStream << endl;
}


void BBCoverage::instrumentFunction(Function &function, DIBuilder &debugBuilder)
{
  // get [optimized] basic blocks to cover in function
  set<BasicBlock*> fBBs;
  if (options.optimizationLevel == OptimizationOption::O0)
    for (Function::iterator i = function.begin(), e = function.end(); i != e; ++i)
      fBBs.insert(&*i);
  else
    fBBs = getOptimizedInstrumentation(function);
  unsigned int arraySize = fBBs.size();
  if (arraySize < 1)
    return;

  BasicBlock &entryBlock = function.getEntryBlock();
  BasicBlock::iterator entryInst = entryBlock.getFirstInsertionPt();

  const CoverageArrays arrays = prepareFunction(function, arraySize, options.silentInternal, debugBuilder);
  
  // instrument each site
  unsigned int curIdx = 0;
  for (set<BasicBlock*>::iterator i = fBBs.begin(), e = fBBs.end(); i != e; ++i)
    {
      BasicBlock &block = **i;

      // find a suitable insertion point; if the entry basic block, we
      // need to be after the array declaration
      IRBuilder<> builder(&block, &block == &entryBlock ? entryInst : block.getFirstInsertionPt());

      // add instrumentation to set local and global coverage bits
      insertArrayStoreInsts(arrays, curIdx, builder);

      // write out the instrumentation site's static location details
      writeOneBB(&block, curIdx++, true);
    }
  
  // write out uninstrumented sites
  unsigned int uninstIdx = 1;
  for (Function::iterator i = function.begin(), e = function.end(); i != e; ++i)
    if (!fBBs.count(&*i))
      writeOneBB(&*i, uninstIdx++, false);
}


bool BBCoverage::runOnModule(Module &module){
  static bool runBefore;
  return runOnModuleOnce(module, options.infoFile, runBefore);
}
