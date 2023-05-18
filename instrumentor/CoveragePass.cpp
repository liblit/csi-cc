//===------------------------- CoveragePass.cpp ---------------------------===//
//
// Superclass for all program coverage passes.  Shared functionality includes:
// formatting of function-level metadata, metadata preliminaries for compilation
// modules, and ensuring that instrumentation occurs only once for each
// function.
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
#include "CoveragePass.h"
#include "CoveragePassNames.h"
#include "InfoFileOption.h"
#include "PrepareCSI.h"
#include "ScopedDIBuilder.h"
#include "Utils.hpp"

#include "llvm_proxy/GlobalVariable.h"
#include "llvm_proxy/Module.h"

using namespace llvm;
using namespace std;


csi_inst::CoveragePass::CoveragePass(char &id, const CoveragePassNames &names)
  : ModulePass(id),
    names(names)
{
}


void csi_inst::CoveragePass::writeFunctionValue(const Function &function, const GlobalVariable &global)
{
  infoStream << '#' << function.getName().str() << '|'
             << global.getName().str() << '\n';
}


bool csi_inst::CoveragePass::prepareForModule(bool &runBefore, const Module &module, const InfoFileOption &infoFile)
{
  if (runBefore)
    return false;
  runBefore = true;

  infoFile.open(infoStream);
  
  // find out if the module contains any instrumentation-enabled functions
  bool anyEnabledFuncs = false;
  PrepareCSI& instData = getAnalysis<PrepareCSI>();
  for (Module::const_iterator i = module.begin(), e = module.end(); i != e; ++i)
    if(instData.hasInstrumentationType(*i, names.upperShort))
      {
        anyEnabledFuncs = true;
        break;
      }

  // if we don't do any instrumentation, no changes are necessary
  if (!anyEnabledFuncs)
    {
      infoStream.close();
      return false;
    }

  return true;
}


void csi_inst::CoveragePass::modulePreliminaries(Module &module, DIBuilder &debugBuilder)
{
  createCompileUnit(debugBuilder, module, *this);
  boolType = createBasicType(debugBuilder, "__" + names.lowerShort + "_bool", 8,
                             dwarf::DW_ATE_boolean);

  // the type of bool
  LLVMContext &Context = module.getContext();
  tBool = Type::getInt8Ty(Context);
}


// make globals and do instrumentation for each function
void csi_inst::CoveragePass::instrumentFunctions(Module &module, DIBuilder &debugBuilder)
{
  const PrepareCSI& plan = getAnalysis<PrepareCSI>();
  for (Module::iterator function = module.begin(), end = module.end(); function != end; ++function)
    if (!function->isDeclaration() && !function->isIntrinsic() &&
        !function->getName().substr(0, 5).equals("__PT_") &&
        plan.hasInstrumentationType(*function, names.upperShort))
      instrumentFunction(*function, debugBuilder);
}


void csi_inst::CoveragePass::getAnalysisUsage(AnalysisUsage &usage) const
{
  usage.setPreservesCFG();
  requireAndPreserve<PrepareCSI>(usage);
}


bool csi_inst::CoveragePass::runOnModuleOnce(Module &module, const InfoFileOption &infoFile, bool &runBefore)
{
  if (!prepareForModule(runBefore, module, infoFile))
    return false;

  // some debug info preliminaries
  ScopedDIBuilder debugBuilder(module);
  modulePreliminaries(module, debugBuilder);
  
  // make globals and do instrumentation for each function
  instrumentFunctions(module, debugBuilder);
  
  infoStream.close();
  return true;
}


////////////////////////////////////////////////////////////////////////


csi_inst::CoveragePass::Options::Options(const CoveragePassNames &names)
  : infoFile(names)
{
}
