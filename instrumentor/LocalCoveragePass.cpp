//===---------------------- LocalCoveragePass.cpp -------------------------===//
//
// Superclass for all program coverage passes that gather stack-local coverage
// data.  Shared functionality includes: declaration of local arrays, and
// storage of boolean data.
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
#include "CoverageOptimization.h"
#include "CoveragePassNames.h"
#include "LocalCoveragePass.h"
#include "Utils.hpp"

#include "llvm_proxy/IRBuilder.h"
#include "llvm_proxy/Module.h"

using namespace llvm;
using namespace std;


csi_inst::LocalCoveragePass::LocalCoveragePass(char &id, const CoveragePassNames &names)
  : CoveragePass(id, names)
{
}


void csi_inst::LocalCoveragePass::getAnalysisUsage(AnalysisUsage &usage) const
{
  CoveragePass::getAnalysisUsage(usage);

  // odd bug necessitates this going last; probably related to
  // <http://llvm.org/bugs/show_bug.cgi?id=9550>
  requireAndPreserve<CoverageOptimizationData>(usage);
}


csi_inst::LocalCoveragePass::CoverageArrays csi_inst::LocalCoveragePass::prepareFunction(Function &function, unsigned arraySize, const SilentInternalOption &silentInternal, DIBuilder &debugBuilder)
{
  ArrayType * const tArr = ArrayType::get(tBool, arraySize);

  // create global coverage array
#if LLVM_VERSION < 30700
  const DIType arrType = createArrayType(debugBuilder, arraySize, boolType);
#else
  DIType * const arrType { createArrayType(debugBuilder, arraySize, boolType) };
#endif
  GlobalVariable &theGlobal = getOrCreateGlobal(debugBuilder, function, *tArr, arrType, names.upperShort);
  
  // declare the local coverage array and set up debug metadata
  AllocaInst * const arrInst = createZeroedLocalArray(function, *tArr, "__" + names.upperShort + "_arr", debugBuilder, boolType, silentInternal);

  // write out the function name and its arrays
  writeFunctionValue(function, theGlobal);

  const CoverageArrays result = { theGlobal, *arrInst };
  return result;
}


void csi_inst::LocalCoveragePass::insertArrayStoreInsts(const CoverageArrays &arrays, unsigned index, IRBuilder<> &builder) const
{
  Type * const intType = builder.getInt32Ty();

  Value * const trueValue = ConstantInt::get(tBool, true);
  Value * const gepIndices[] = {
    Constant::getNullValue(intType),
    ConstantInt::get(intType, index),
  };

  Value * const localGEP = builder.CreateInBoundsGEP(&arrays.local, gepIndices, "local"  + names.upperShort);
  StoreInst * const localStore = builder.CreateStore(trueValue, localGEP, true);
  Value * const globalGEP = builder.CreateInBoundsGEP(&arrays.global, gepIndices, "global" + names.upperShort);
#if LLVM_VERSION < 30200
  StoreInst * const globalStore = builder.CreateStore(trueValue, globalGEP, false);
  globalStore->setAlignment(1);
#else
  StoreInst * const globalStore = builder.CreateAlignedStore(trueValue, globalGEP, 1, false);
#endif
#if LLVM_VERSION < 30900
  globalStore->setOrdering(Unordered);
#else
  globalStore->setOrdering(AtomicOrdering::Unordered);
#endif
#if LLVM_VERSION < 50000
  globalStore->setSynchScope(SynchronizationScope::CrossThread);
#else
  globalStore->setSyncScopeID(SyncScope::System);
#endif

  // clear out debug data for instrumentation instructions (so as not to
  // confuse CFG writing into thinking these are from the original code).
  // Sadly, it appears there is no way to clear debug data from the IRBuilder.
  if(Instruction* localGEPInst = dyn_cast<Instruction>(localGEP))
    localGEPInst->setDebugLoc(DebugLoc());
  if(Instruction* globalGEPInst = dyn_cast<Instruction>(globalGEP))
    globalGEPInst->setDebugLoc(DebugLoc());
  localStore->setDebugLoc(DebugLoc());
  globalStore->setDebugLoc(DebugLoc());

  attachCSILabelToInstruction(*globalStore, indexToLabel(index));
}

string csi_inst::LocalCoveragePass::indexToLabel(unsigned int index) const
{
  return(names.upperShort + csi_inst::to_string(index));
}


////////////////////////////////////////////////////////////////////////


csi_inst::LocalCoveragePass::Options::Options(const CoveragePassNames &names, const char descriptionO1[])
  : CoveragePass::Options(names),
    optimizationLevel(names, descriptionO1),
    silentInternal(names)
{
}
