//===--------------------- llvm_proxy/TypeBuilder.h -----------------------===//
//
// A proxy header file to cleanly support different versions of LLVM.
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
#include "../Versions.h"

#if LLVM_VERSION < 30200
  #include <llvm/Support/TypeBuilder.h>
#elif LLVM_VERSION < 30300
  #include <llvm/TypeBuilder.h>
#else
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #include <llvm/IR/TypeBuilder.h>
  #pragma GCC diagnostic pop
#endif
