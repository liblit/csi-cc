//===---------------------- OptimizationOption.cpp ------------------------===//
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
#include "CoveragePassNames.h"
#include "OptimizationOption.h"
#include "optionName.h"
#include "Utils.hpp"

using namespace llvm;
using namespace std;


csi_inst::OptimizationOption::OptimizationOption(const CoveragePassNames &names, const char descriptionO1[])
  : flag(names.lowerShort + "-opt"),
    description(names.titleFull + " Coverage Optimization Level:"),
    option(optionName(flag),
           cl::desc(description.c_str()),
           cl::init(O2),
           cl::values(
             clEnumValN(O0, "0", "none"),
             clEnumValN(O1, "1", descriptionO1),
             clEnumValN(O2, "2", "(default) locally-minimal approximation"),
             clEnumValN(O3, "3", "fully optimal (GAMS- or LEMON-based) optimization")
             CL_ENUM_VAL_END
          )
     )
{
}
