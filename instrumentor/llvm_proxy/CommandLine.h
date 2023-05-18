//===-------------------- llvm_proxy/CommandLine.h ------------------------===//
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if !defined(__GNUC__) || __GNUC__ > 5
  #pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include <llvm/Support/CommandLine.h>
#pragma GCC diagnostic pop
