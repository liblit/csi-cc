//===---------------------------- Utils.cpp -------------------------------===//
//
// Utilities providing convenience functions for various tasks not specific to
// any particular instrumentation pass.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2016 Peter J. Ohmann and Benjamin R. Liblit
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

#define DEBUG_TYPE "utilities"

#include "Utils.hpp"

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/Module.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IRBuilder.h"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#if LLVM_VERSION < 30200
#include <llvm/Target/TargetData.h>
#endif

#include <sstream>

using namespace csi_inst;
using namespace llvm;
using namespace std;


static string nameMash(string mName){
  size_t mDot = mName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890");
  while(mDot != string::npos){
    mName.replace(mDot, 1, "_");
    mDot = mName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890");
  }
  return(mName);
}


string csi_inst::getUniqueCFunctionName(const Function &F)
{
  const DebugLoc dbLoc = findEarlyDebugLoc(F, true);
  
  if(dbLoc.isUnknown())
    return nameMash(F.getParent()->getModuleIdentifier()) + '_' +
           F.getName().str();
  
  const LLVMContext &context = F.getParent()->getContext();
  DISubprogram sp = getDISubprogram(dbLoc.getScope(context));
  if(!sp.isSubprogram())
    return nameMash(F.getParent()->getModuleIdentifier()) + '_' +
           F.getName().str();
  
  return nameMash(sp.getFilename().str()) + '_' + F.getName().str();
}


string csi_inst::setBB_asstring(set<BasicBlock*> theSet){
  string result;
  for(set<BasicBlock*>::iterator i = theSet.begin(), e = theSet.end(); i != e; ++i){
    if(i != theSet.begin())
      result += ",";
    if(*i == NULL)
      result += "NULL";
    else
      result += (*i)->getName().str();
  }
  return(result);
}


GlobalVariable &csi_inst::getOrCreateGlobal(DIBuilder &debugBuilder, Function &function,
					    Type &type, const DIType &typeInfo,
                                            const string &upperShortName)
{
  // mangle up a unique name
  const string fName = getUniqueCFunctionName(function);
  const string globalName = "__" + upperShortName + "_arr_" +
    fName.substr(0, ((fName.find("$") == string::npos)
		     ? fName.size()
		     : fName.find("$")));

  // check for existing global
  Module &module = *function.getParent();
  GlobalVariable *preexisting = module.getGlobalVariable(globalName, false);
  if (preexisting)
    {
      if (preexisting->getType()->getElementType() != &type)
	report_fatal_error("unable to get or create coverage global variable for '" + globalName + "' for function '" + function.getName() + "'");
      else
	return *preexisting;
    }

  // create new global
  const GlobalValue::LinkageTypes linkage = function.hasAvailableExternallyLinkage()
    ? GlobalValue::WeakAnyLinkage
    : GlobalValue::ExternalLinkage;
  Constant * const initializer = Constant::getNullValue(&type);
  GlobalVariable &result = *new GlobalVariable(module, &type, false, linkage, initializer, globalName);
  createGlobalVariable(debugBuilder, typeInfo, result);
  return result;
}

DIType csi_inst::createArrayType(DIBuilder &builder, uint64_t count, const DIType &elementType)
{
  const uint64_t elementSizeInBits = elementType.getSizeInBits();
#if LLVM_VERSION < 30600
  Value * const subscript = builder.getOrCreateSubrange(0, count - (LLVM_VERSION < 30300));
  const DIArray subscriptArray = builder.getOrCreateArray(subscript);
#else
  Metadata * const subscript[] = { builder.getOrCreateSubrange(0, count) };
  const DIArray subscriptArray = builder.getOrCreateArray(subscript);
#endif
  return builder.createArrayType(count * elementSizeInBits, elementSizeInBits, elementType, subscriptArray);
}


DIGlobalVariable csi_inst::createGlobalVariable(DIBuilder &builder, const DIType &typeInfo, GlobalVariable &global)
{
  const StringRef name = global.getName();
#if LLVM_VERSION < 30600
  return builder.createGlobalVariable(name, DIFile(), 0, typeInfo, false, &global);
#else
  return builder.createGlobalVariable(DIFile(), name, name, DIFile(), 0, typeInfo, false, &global);
#endif
}


DebugLoc csi_inst::findEarlyDebugLoc(const Function &function, bool silent)
{
  // Iterate over all instructions.  First instructions seen will be
  // those from the entry block, which is where we want to find a
  // debug location if possible.  If we don't find one there, though,
  // then we'll simply continue on to instructions from other basic
  // blocks.

  for (const_inst_iterator instruction = inst_begin(function), end = inst_end(function); instruction != end; ++instruction)
    {
      const DebugLoc &location = instruction->getDebugLoc();
      if (!location.isUnknown())
	{
	  if (!silent && instruction->getParent() != &function.getEntryBlock())
	    DEBUG(dbgs() << "WARNING: debug location outside of entry block used "
		  << "for instrumented function " << function.getName() << '\n');
	  return location;
	}
    }

  if (!silent)
    errs() << "WARNING: there will be no debug locations for instrumented"
	   << " function " << function.getName() << '\n';
  return DebugLoc();
}


Instruction *csi_inst::insertDeclare(DIBuilder &builder, Value *var, DIVariable varInfo, const DebugLoc &location, Instruction *before)
{
#if LLVM_VERSION < 30600
  Instruction * const declaration = builder.insertDeclare(var, varInfo, before);
#else
  Instruction * const declaration = builder.insertDeclare(var, varInfo, builder.createExpression(), before);
#endif
  declaration->setDebugLoc(location);
  return declaration;
}


static uint64_t getTypeStoreSize(const Module &module, Type &type)
{
#if LLVM_VERSION < 30200
  return TargetData(&module).getTypeStoreSize(&type);
#elif LLVM_VERSION < 30500
  return DataLayout(&module).getTypeStoreSize(&type);
#else
  return module.getDataLayout()->getTypeStoreSize(&type);
#endif
}


AllocaInst *csi_inst::createZeroedLocalArray(Function &function, ArrayType &arrayType, const string &name, DIBuilder &debugBuilder, const DIType &elementTypeInfo, bool silent)
{
  // find proper insertion point for new alloca and other supporting instructions
  const BasicBlock::iterator entryInst = function.getEntryBlock().getFirstInsertionPt();
  IRBuilder<> builder(entryInst);

  // allocate stack space for array and zero-initialize all elements
  const Module &module = *function.getParent();
  AllocaInst * const arrayAllocation = builder.CreateAlloca(&arrayType, NULL, name);
  const uint64_t sizeInBytes = getTypeStoreSize(module, arrayType);
  builder.CreateMemSet(arrayAllocation, builder.getInt8(0), sizeInBytes, 0, true);

  // set up debug metadata
  const DebugLoc &location = findEarlyDebugLoc(function, silent);
  if (!location.isUnknown()) {
    const uint64_t elementCount = arrayType.getNumElements();
    const DIType arrayTypeInfo = createArrayType(debugBuilder, elementCount, elementTypeInfo);
    const LLVMContext &context = module.getContext();
    const MDNode * const scope = location.getScope(context);
    const DIVariable arrayVarInfo = debugBuilder.createLocalVariable(dwarf::DW_TAG_auto_variable, DIDescriptor(scope), name, DIFile(scope), 0, arrayTypeInfo);
    insertDeclare(debugBuilder, arrayAllocation, arrayVarInfo, location, entryInst);
  }

  // return allocated array to caller for further use
  return arrayAllocation;
}

namespace csi_inst{
  template<typename T>
  static string to_string(T val){
    static stringstream theStream;
    theStream.str(string());
    theStream << val;
    return(theStream.str());
  }
}

string csi_inst::to_string(int val){
  return(csi_inst::to_string<int>(val));
}

string csi_inst::to_string(unsigned int val){
  return(csi_inst::to_string<unsigned int>(val));
}
