//===---------------------------- Versions.h ------------------------------===//
//
// This file grabs version information for LLVM and the building compiler.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2013 Peter J. Ohmann and Benjamin R. Liblit
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

#include <llvm/Config/llvm-config.h>
#if !defined(LLVM_VERSION_MAJOR) || LLVM_VERSION_MAJOR != 3 || \
    !defined(LLVM_VERSION_MINOR) || (LLVM_VERSION_MINOR != 1 && \
                                     LLVM_VERSION_MINOR != 2 && \
                                     LLVM_VERSION_MINOR != 3)
  #error "LLVM version not supported.  Supported versions are <3.1, 3.2, 3.3>"
#endif

#define LLVM_3_1 (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 1)
#define LLVM_3_2 (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 2)
#define LLVM_3_3 (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 3)

#define GCC_VERSION (__GNUC__ * 10000 \
                   + __GNUC_MINOR__ * 100 \
                   + __GNUC_PATCHLEVEL__)

#endif
