//===------------------------- ScopedDIBuilder.h --------------------------===//
//
// A debug info builder that automatically finalizes itself when destroyed.
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
#ifndef CSI_SCOPED_DI_BUILDER_H
#define CSI_SCOPED_DI_BUILDER_H

#include "llvm_proxy/DIBuilder.h"


namespace csi_inst {
  class ScopedDIBuilder : public llvm::DIBuilder {
  public:

#if __cplusplus >= 201103L
    using DIBuilder::DIBuilder;
#else
    // no inherited constructors before C++11
    ScopedDIBuilder(llvm::Module &module) : DIBuilder(module) { }
#endif

    ~ScopedDIBuilder() { finalize(); }
  };
}


#endif
