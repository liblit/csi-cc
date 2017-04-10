#ifndef INCLUDE_OPTION_NAME_H
#define INCLUDE_OPTION_NAME_H

#include "Versions.h"

#include <string>


#if LLVM_VERSION < 40000


inline const char *optionName(const std::string &string)
{
  return string.c_str();
}


#else


#include <llvm/ADT/StringRef.h>

inline llvm::StringRef optionName(const std::string &string)
{
  return string;
}

#endif


#endif
