//===----------------------- OptimizationOption.h -------------------------===//
//
// A wrapper class for coverage optimization options.
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
#ifndef CSI_OPTIMIZATION_LEVEL_H
#define CSI_OPTIMIZATION_LEVEL_H

#include "llvm_proxy/CommandLine.h"

#include <string>


namespace csi_inst
{
  struct CoveragePassNames;


  class OptimizationOption
  {
  public:
    enum Level
      {
        O0,
        O1,
        O2,
        O3,
      };

    OptimizationOption(const CoveragePassNames &, const char descriptionO1[]);
    operator Level() const;

  private:
    const std::string flag;
    const std::string description;
    llvm::cl::opt<Level> option;
  };
}


////////////////////////////////////////////////////////////////////////


inline csi_inst::OptimizationOption::operator Level() const
{
  return option;
}


#endif // !CSI_OPTIMIZATION_LEVEL_H
