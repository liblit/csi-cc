//===------------------------ InfoFileOption.cpp --------------------------===//
//
// A wrapper class to indicate output metadata files for instrumentation
// passes, and manage their output streams.
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
#define DEBUG_TYPE "info-file-option"

#include "CoveragePassNames.h"
#include "InfoFileOption.h"
#include "optionName.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>

#include <fstream>

using namespace llvm;
using namespace std;


csi_inst::InfoFileOption::InfoFileOption(const CoveragePassNames &names)
  : lowerShortName(names.lowerShort),
    flag(names.lowerShort + "-info-file"),
    description("The path to the " + names.lowerFull + " coverage info file."),
    option(optionName(flag),
           cl::desc(description.c_str()),
           cl::value_desc("file_path"))
{
}


void csi_inst::InfoFileOption::open(ofstream &stream) const
{
  const string &value = option.getValue();
  if (value.empty())
    report_fatal_error(lowerShortName + " cannot continue: " + option.ArgStr + " [" + option.ValueStr + "] is required", false);

  stream.open(value.c_str(), ios::out | ios::trunc);
  if (!stream)
    report_fatal_error("unable to open " + lowerShortName + "-file location: " + value, false);
  DEBUG(dbgs() << "Output stream opened to " << value << "\n");
}
