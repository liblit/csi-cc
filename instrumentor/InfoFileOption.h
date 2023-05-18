//===------------------------- InfoFileOption.h ---------------------------===//
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
#ifndef CSI_INFO_FILE_H
#define CSI_INFO_FILE_H

#include "llvm_proxy/CommandLine.h"

#include <fstream>
#include <string>


namespace csi_inst
{
  struct CoveragePassNames;


  class InfoFileOption
  {
  public:
    InfoFileOption(const CoveragePassNames &);
    void open(std::ofstream &) const;

  private:
    const std::string lowerShortName;
    const std::string flag;
    const std::string description;
    llvm::cl::opt<std::string> option;
  };
}


#endif // !CSI_INFO_FILE_H
