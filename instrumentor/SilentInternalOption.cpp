//===--------------------- SilentInternalOption.cpp -----------------------===//
//
// A simple class enacapulating the "silent" flags for each instrumentation
// pass.  Each pass has its own flag to silence warnings, but the front-end
// just has one command-line for this purpose.
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
#include "optionName.h"
#include "SilentInternalOption.h"

using namespace llvm;
using namespace std;


csi_inst::SilentInternalOption::SilentInternalOption(const CoveragePassNames &names)
  : flag(names.lowerShort + "-silent"),
    description("Silence internal warnings.  Will still print errors that cause " + names.upperShort + " to fail."),
    option(optionName(flag), cl::desc(description.c_str()))
{
}
