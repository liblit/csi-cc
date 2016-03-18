//===------------------------- CallCoverage.cpp ---------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
// gathering both global and local coverage information.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2016 Peter J. Ohmann and Benjamin R. Liblit
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
   names
);

// Register call coverage as a pass
char CallCoverage::ID = 0;
static RegisterPass<CallCoverage> X("call-coverage",
                "Insert call coverage instrumentation",
                false, false);


void CallCoverage::writeOneCall(CallInst* theCall, unsigned int index,
                                bool isInstrumented){
  unsigned int lineNum = 0;
  DebugLoc dbLoc = theCall->getDebugLoc();
  if(!dbLoc.isUnknown())
    lineNum = dbLoc.getLine();
  Function* calledFn = theCall->getCalledFunction();
  const string fnName = calledFn ? calledFn->getName() : "?";
  infoStream << (isInstrumented ? "" : "-") << index << '|' << lineNum << '|'
             << fnName << '\n';
}

void CallCoverage::instrumentFunction(Function &function)
{
  // find all of the callsites in function
  set<CallInst*> fCalls;
  const ExtrinsicCalls<inst_iterator> calls = extrinsicCalls(function);
  for (ExtrinsicCalls<inst_iterator>::iterator call = calls.begin(); call != calls.end(); ++call)
    fCalls.insert(call);

  // make globals and do instrumentation for each function
  unsigned int arraySize = fCalls.size();
  if (arraySize < 1)
    return;

  const CoverageArrays arrays = prepareFunction(function,
                                                arraySize,
                                                options.silentInternal);
  
  // instrument each site
  unsigned int curIdx = 0;
  for (set<CallInst*>::iterator i = fCalls.begin(), e = fCalls.end(); i != e; ++i)
    {
      CallInst &call = **i;
      assert(!call.getCalledFunction() ||
             !call.getCalledFunction()->isIntrinsic());

      // add instrumentation to set local and global coverage bits
      IRBuilder<> builder(nextInst(&call));
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
