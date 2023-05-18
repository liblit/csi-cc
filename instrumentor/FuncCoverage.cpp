//===------------------------- FuncCoverage.cpp ---------------------------===//
//
// This pass instruments function entry points for interprocedural
// analysis by gathering global function coverage information.
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
#define DEBUG_TYPE "func-coverage"

#include "CoveragePassNames.h"
#include "FuncCoverage.h"
#include "PrepareCSI.h"
#include "ScopedDIBuilder.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IntrinsicInst.h"
#include "llvm_proxy/Module.h"

#include <iostream>

using namespace csi_inst;
using namespace llvm;
using namespace std;


const CoveragePassNames FuncCoverage::names = {
  "fc",
  "FC",
  "function",
  "Function",
};

FuncCoverage::Options FuncCoverage::options(names);

// Register function coverage as a pass
char FuncCoverage::ID = 0;
static RegisterPass<FuncCoverage> X("fn-coverage",
                "Insert function coverage instrumentation",
                false, false);


void FuncCoverage::instrumentFunction(Function &function, DIBuilder &debugBuilder)
{
  // create new global variable to hold this function's coverage bit
  GlobalVariable &theGlobal = getOrCreateGlobal(debugBuilder, function, *tBool, boolType, names.upperShort);

  // write out the function name and its arrays
  writeFunctionValue(function, theGlobal);
  
  // instrument the function's entry
  Instruction* insertPoint = &*function.getEntryBlock().getFirstInsertionPt();
  new StoreInst(ConstantInt::get(tBool, 1), &theGlobal, false, 1,
#if LLVM_VERSION < 30900
                Unordered,
#else
                AtomicOrdering::Unordered,
#endif
#if LLVM_VERSION < 50000
		CrossThread,
#else
                SyncScope::System,
#endif
		insertPoint);
}


bool FuncCoverage::runOnModule(Module& module)
{
  static bool runBefore;
  return runOnModuleOnce(module, options.infoFile, runBefore);
}
