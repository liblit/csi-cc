//===-------------------------- CoveragePass.h ----------------------------===//
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
#ifndef CSI_COVERAGE_PASS_H
#define CSI_COVERAGE_PASS_H

#include "InfoFileOption.h"
#include "Versions.h"

#include <llvm/Pass.h>

#include "llvm_proxy/DebugInfo.h"

#include <fstream>
#include <map>

namespace llvm
{
  class DIBuilder;
  class GlobalVariable;
}


namespace csi_inst
{
  struct CoveragePassNames;


  class CoveragePass : public llvm::ModulePass
  {
  private:
    bool prepareForModule(bool &, const llvm::Module &, const InfoFileOption &);
    void modulePreliminaries(llvm::Module &, llvm::DIBuilder &);
    void instrumentFunctions(llvm::Module &, llvm::DIBuilder &);
    virtual void instrumentFunction(llvm::Function &, llvm::DIBuilder &) = 0;

  protected:
    const CoveragePassNames &names;

    struct Options
    {
      InfoFileOption infoFile;
      Options(const CoveragePassNames &);
    };

    // info file (managed by runOnFunction and written to as we go)
    std::ofstream infoStream;

    // type used for storing coverage bits
    llvm::Type *tBool;
#if LLVM_VERSION < 30700
    llvm::DIType boolType;
#else
    llvm::DIType *boolType;
#endif

    CoveragePass(char &, const CoveragePassNames &);

    template <typename Pass> static void requireAndPreserve(llvm::AnalysisUsage &);

    bool runOnModuleOnce(llvm::Module &, const InfoFileOption &, bool &);
    void writeFunctionValue(const llvm::Function &, const llvm::GlobalVariable &);

  public:
    void getAnalysisUsage(llvm::AnalysisUsage &) const;
  };
}


////////////////////////////////////////////////////////////////////////


template <typename PassType> void csi_inst::CoveragePass::requireAndPreserve(llvm::AnalysisUsage &usage)
{
  usage.addRequired<PassType>();
  usage.addPreserved<PassType>();
}


#endif // !CSI_COVERAGE_PASS_H
