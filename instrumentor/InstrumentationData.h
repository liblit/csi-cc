//===---------------------- InstrumentationData.h -------------------------===//
//
// This module contains utilities for CSI instrumentation preparation.
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
#ifndef CSI_INSTRUMENTATION_DATA_H
#define CSI_INSTRUMENTATION_DATA_H

#include "llvm_proxy/CFG.h"

#include <string>
#include <set>
#include <vector>

namespace csi_inst {

// A vector of filters to apply over proposed schemes and functions they are
// to be applied to.  Filters may modify the scheme, but only by removing
// instrumentors.
// @return true if the scheme was modified; false otherwise.
typedef bool(*FilterFn)(std::set<std::string>& scheme, llvm::Function* F);
extern const std::vector<FilterFn> Filters;

extern const std::set<std::string> Instrumentors;

} // end csi_inst namespace

#endif
