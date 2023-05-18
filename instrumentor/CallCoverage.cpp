//===------------------------- CallCoverage.cpp ---------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
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
#define DEBUG_TYPE "call-coverage"

#include "CallCoverage.h"
#include "CoveragePassNames.h"
#include "ExtrinsicCalls.h"
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


const CoveragePassNames CallCoverage::names = {
  "cc",
  "CC",
  "call",
  "Call",
};

CallCoverage::Options CallCoverage::options(
   names,
   "multiple calls within a single basic block"
);

// Register call coverage as a pass
char CallCoverage::ID = 0;
static RegisterPass<CallCoverage> X("call-coverage",
                "Insert call coverage instrumentation",
                false, false);


set<CallInst*> CallCoverage::selectCalls(const set<BasicBlock*>& bbs)
{
  set<CallInst*> result;
  for (set<BasicBlock*>::iterator bb = bbs.begin(), bbe = bbs.end(); bb != bbe; ++bb)
    {
      const ExtrinsicCalls<BasicBlock::iterator> calls = extrinsicCalls(**bb);
      if (calls.begin() != calls.end())
        result.insert(calls.begin());
      else
        report_fatal_error("attempt to select a call instruction in basic "
                           "block '" + (*bb)->getName() + "' which has none");
    }
  return result;
}


set<BasicBlock*> CallCoverage::getBBsForCalls(const set<CallInst*>& calls){
  set<BasicBlock*> result;
  for (set<CallInst*>::iterator i = calls.begin(), e = calls.end(); i != e; ++i)
    {
      if((*i)->getParent() == NULL)
        report_fatal_error("call coverage encountered a call instruction not embedded in a basic block");
      result.insert((*i)->getParent());
    }
  return result;
}

void CallCoverage::writeOneCall(CallInst* theCall, unsigned int index,
                                bool isInstrumented){
  unsigned int lineNum = 0;
  DebugLoc dbLoc = theCall->getDebugLoc();
  if (!isUnknown(dbLoc))
    lineNum = dbLoc.getLine();
  Function* calledFn = theCall->getCalledFunction();
  const string fnName = calledFn ? calledFn->getName() : "?";
  infoStream << (isInstrumented ? "" : "-") << index << '|'
             << (isInstrumented ? indexToLabel(index) : "") << '|'
             << lineNum << '|'
             << fnName << '\n';
}

void CallCoverage::instrumentFunction(Function &function, DIBuilder &debugBuilder)
{
  // find all of the callsites in function
  set<CallInst*> fCalls;
  const ExtrinsicCalls<inst_iterator> calls = extrinsicCalls(function);
  for (ExtrinsicCalls<inst_iterator>::iterator call = calls.begin(); call != calls.end(); ++call)
    fCalls.insert(call);

  // get the calls we'll use based on the optimization level
  switch(options.optimizationLevel)
    {
    case OptimizationOption::O0:
      break;
    case OptimizationOption::O1:
    case OptimizationOption::O2:
    case OptimizationOption::O3: {
      set<BasicBlock*> callBBs = getBBsForCalls(fCalls);
      if(options.optimizationLevel == OptimizationOption::O1){
        fCalls = selectCalls(callBBs);
        if(fCalls.size() != callBBs.size())
          report_fatal_error("call coverage encountered an internal error "
                             "selecting single calls for basic blocks in "
                             "function '" + function.getName() + "'");
        break;
      }
      
      // here: O2 or O3
      CoverageOptimizationData& sgData =
         getAnalysis<CoverageOptimizationData>(function);
      
      set<BasicBlock*> allBBs;
      for(Function::iterator i = function.begin(), e = function.end(); i != e; ++i)
        allBBs.insert(&*i);
      
      set<BasicBlock*> result;
      if(options.optimizationLevel == OptimizationOption::O2){
        // here: O2
        // NOTE: currently using (I=calls, D=calls) for less reliance on LLVM BB costs
        result = sgData.getOptimizedProbes(&function, &callBBs, &callBBs);
      }
      else{
        // here: O3
        // NOTE: currently using (I=calls, D=calls) for less reliance on LLVM BB costs
#if defined(USE_GAMS) || defined(USE_LEMON)
        result = sgData.getOptimizedProbes(&function, &callBBs, &callBBs, true);
#else
        report_fatal_error("csi build does not support optimization level 3. "
                           "csi must be built with GAMS or LEMON optimization "
                           "enabled");
#endif
      }

      DEBUG(dbgs() << "instrumenting: " << setBB_asstring(result) << '\n');

      fCalls = selectCalls(result);
      if(fCalls.size() != result.size())
        report_fatal_error("call coverage encountered an internal error "
                           "selecting single calls for basic blocks in "
                           "function '" + function.getName() + "'");
      break;
    }
    default:
      report_fatal_error("internal error allowing bad call coverage "
                         "optimization value");
    }
  
  // make globals and do instrumentation for each function
  unsigned int arraySize = fCalls.size();
  if (arraySize < 1)
    return;

  const CoverageArrays arrays = prepareFunction(function,
                                                arraySize,
                                                options.silentInternal,
                                                debugBuilder);
  
  // instrument each site
  unsigned int curIdx = 0;
  for (set<CallInst*>::iterator i = fCalls.begin(), e = fCalls.end(); i != e; ++i)
    {
      CallInst &call = **i;
      assert(!call.getCalledFunction() ||
             !call.getCalledFunction()->isIntrinsic());

      // add instrumentation to set local and global coverage bits
      IRBuilder<> builder(&*nextInst(&call));
      insertArrayStoreInsts(arrays, curIdx, builder);
    
      // write out the instrumentation site's static location details
      writeOneCall(&call, curIdx++, true);
    }
  
  // write out uninstrumented sites
  unsigned int uninstIdx = 1;
  for (ExtrinsicCalls<inst_iterator>::iterator call = calls.begin(); call != calls.end(); ++call)
    if (!fCalls.count(call))
      writeOneCall(call, uninstIdx++, false);
}


bool CallCoverage::runOnModule(Module &module){
  static bool runBefore;
  return runOnModuleOnce(module, options.infoFile, runBefore);
}
