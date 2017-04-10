#ifndef INCLUDE_PASS_NAME_H
#define INCLUDE_PASS_NAME_H

#include "Versions.h"

#if LLVM_VERSION < 40000

typedef const char *PassName;

#else

namespace llvm {
  class StringRef;
}

typedef llvm::StringRef PassName;

#endif

#endif
