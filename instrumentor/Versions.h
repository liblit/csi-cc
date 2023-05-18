//===---------------------------- Versions.h ------------------------------===//
//
// This file grabs version information for LLVM and the building compiler.
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

#ifndef CSI_VERSIONS_H
#define CSI_VERSIONS_H

#include "config.h"

#if LLVM_CONFIG_VERSION < 30800
// LLVM 3.7 and earlier
#  include <llvm/Config/config.h>
#else
// LLVM 3.8 and later
#  include <llvm/Config/llvm-config.h>
#endif

#ifndef LLVM_VERSION_PATCH
#  define LLVM_VERSION_PATCH 0
#endif


#define LLVM_VERSION (LLVM_VERSION_MAJOR * 10000 \
                   + LLVM_VERSION_MINOR * 100 \
                   + LLVM_VERSION_PATCH)

#define GCC_VERSION (__GNUC__ * 10000 \
                   + __GNUC_MINOR__ * 100 \
                   + __GNUC_PATCHLEVEL__)

#endif
