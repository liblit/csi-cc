//===---------------------------- Utils.cpp -------------------------------===//
//
// Utilities providing convenience functions for various tasks not specific to
// any particular instrumentation pass.
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

#define DEBUG_TYPE "utilities"

#include "PassName.h"
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
  
  if (isUnknown(dbLoc))
    return nameMash(F.getParent()->getModuleIdentifier()) + '_' +
           F.getName().str();
  
#if LLVM_VERSION < 30700
  const LLVMContext &context = F.getParent()->getContext();
  DISubprogram sp = getDISubprogram(dbLoc.getScope(context));
  if(!sp.isSubprogram())
    return nameMash(F.getParent()->getModuleIdentifier()) + '_' +
           F.getName().str();
  
  return nameMash(sp.getFilename().str()) + '_' + F.getName().str();

#else

  const DISubprogram * const sp { getDISubprogram(dbLoc.getScope()) };
  if(!sp)
    return nameMash(F.getParent()->getModuleIdentifier()) + '_' +
           F.getName().str();
  
  return nameMash(sp->getFilename().str()) + '_' + F.getName().str();

#endif
}


string csi_inst::setBB_asstring(set<BasicBlock*> theSet){
  string result;
  for(set<BasicBlock*>::iterator i = theSet.begin(), e = theSet.end(); i != e; ++i){
    if(i != theSet.begin())
      result += ',';
    if(*i == NULL)
      result += "NULL";
    else
      result += (*i)->getName().str();
  }
  return(result);
}


GlobalVariable &csi_inst::getOrCreateGlobal(DIBuilder &debugBuilder,
                                            Function &function,
                                            Type &type,
#if LLVM_VERSION < 30700
                                            const DIType &typeInfo,
#else
                                            DIType *typeInfo,
#endif
                                            const string &upperShortName)
{
  // mangle up a unique name
  const string fName = getUniqueCFunctionName(function);
  const string globalName = "__" + upperShortName + "_arr_" +
    fName.substr(0, ((fName.find('$') == string::npos)
                     ? fName.size()
                     : fName.find('$')));

  // check for existing global
  Module &module = *function.getParent();
  GlobalVariable *preexisting = module.getGlobalVariable(globalName, false);
  if (preexisting)
    {
      if (preexisting->getType()->getElementType() != &type)
        report_fatal_error("unable to get or create coverage global variable "
                           "for '" + globalName + "' for function '" +
                           function.getName() + "'");
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


#if LLVM_VERSION < 30600
DIVariable csi_inst::createAutoVariable(DIBuilder &builder, DIDescriptor scope, StringRef name, DIFile file, unsigned lineNo, DIType ty, bool alwaysPreserve)
{
  return builder.createLocalVariable(dwarf::DW_TAG_auto_variable, scope, name, file, lineNo, ty, alwaysPreserve);
}

#elif LLVM_VERSION < 30700
DIVariable csi_inst::createAutoVariable(DIBuilder &builder, DIDescriptor scope, StringRef name, DIFile file, unsigned lineNo, DITypeRef ty, bool alwaysPreserve)
{
  return builder.createLocalVariable(dwarf::DW_TAG_auto_variable, scope, name, file, lineNo, ty, alwaysPreserve);
}

#elif LLVM_VERSION < 30800
DILocalVariable *csi_inst::createAutoVariable(DIBuilder &builder, DIScope *scope, StringRef name, DIFile *file, unsigned lineNo, DIType *ty, bool alwaysPreserve)
{
  return builder.createLocalVariable(dwarf::DW_TAG_auto_variable, scope, name, file, lineNo, ty, alwaysPreserve);
}

#else
DILocalVariable *csi_inst::createAutoVariable(DIBuilder &builder, DIScope *scope, StringRef name, DIFile *file, unsigned lineNo, DIType *ty, bool alwaysPreserve)
{
  return builder.createAutoVariable(scope, name, file, lineNo, ty, alwaysPreserve);
}

#endif


#if LLVM_VERSION < 30700


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


void csi_inst::createGlobalVariable(DIBuilder &builder, const DIType &typeInfo, GlobalVariable &global)
{
  const StringRef name = global.getName();
#if LLVM_VERSION < 30600
  builder.createGlobalVariable(name, DIFile(), 0, typeInfo, false, &global);
#else
  builder.createGlobalVariable(DIFile(), name, name, DIFile(), 0, typeInfo, false, &global);
#endif
}


#else

DIType *csi_inst::createArrayType(DIBuilder &builder, uint64_t count, DIType *elementType)
{
  const uint64_t elementSizeInBits { elementType->getSizeInBits() };
  Metadata * const subscript[] { builder.getOrCreateSubrange(0, count) };
  const DINodeArray subscriptArray { builder.getOrCreateArray(subscript) };
  return builder.createArrayType(count * elementSizeInBits, elementSizeInBits, elementType, subscriptArray);
}


void csi_inst::createGlobalVariable(DIBuilder &builder, DIType *typeInfo, GlobalVariable &global)
{
  const StringRef name { global.getName() };
#if LLVM_VERSION < 40000
  builder.createGlobalVariable(nullptr, name, name, nullptr, 0, typeInfo, false, &global);
#else
  builder.createGlobalVariableExpression(nullptr, name, name, nullptr, 0, typeInfo, false, nullptr);
#endif
}


#endif


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
      if (!isUnknown(location))
      {
        if (!silent && instruction->getParent() != &function.getEntryBlock())
          DEBUG(dbgs() << "WARNING: debug location outside of entry block used "
                       << "for instrumented function "
                       << function.getName() << '\n');
        return location;
      }
    }

  if (!silent)
    errs() << "WARNING: there will be no debug locations for instrumented"
           << " function " << function.getName() << '\n';
  return DebugLoc();
}


#if LLVM_VERSION < 30700


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


#else


Instruction *csi_inst::insertDeclare(DIBuilder &builder, Value *var, DILocalVariable *varInfo, const DebugLoc &location, Instruction *before)
{
  return builder.insertDeclare(var, varInfo, builder.createExpression(), location, before);
}


#endif


static uint64_t getTypeStoreSize(const Module &module, Type &type)
{
#if LLVM_VERSION < 30200
  return TargetData(&module).getTypeStoreSize(&type);
#elif LLVM_VERSION < 30500
  return DataLayout(&module).getTypeStoreSize(&type);
#elif LLVM_VERSION < 30700
  return module.getDataLayout()->getTypeStoreSize(&type);
#else
  return module.getDataLayout().getTypeStoreSize(&type);
#endif
}


AllocaInst *csi_inst::createZeroedLocalArray(Function &function,
                                             ArrayType &arrayType,
                                             const string &name,
                                             DIBuilder &debugBuilder,
#if LLVM_VERSION < 30700
                                             const DIType &elementTypeInfo,
#else
                                             DIType *elementTypeInfo,
#endif
                                             bool silent)
{
  // find proper insertion point for new alloca and other supporting instructions
  const BasicBlock::iterator entryInst = function.getEntryBlock().getFirstInsertionPt();
  IRBuilder<> builder(&*entryInst);

  // allocate stack space for array and zero-initialize all elements
  const Module &module = *function.getParent();
  AllocaInst * const arrayAllocation = builder.CreateAlloca(&arrayType, NULL, name);
  const uint64_t sizeInBytes = getTypeStoreSize(module, arrayType);
  builder.CreateMemSet(arrayAllocation, builder.getInt8(0), sizeInBytes, 0, true);

  // set up debug metadata
  const DebugLoc &location = findEarlyDebugLoc(function, silent);
  if (!isUnknown(location)) {
    const uint64_t elementCount = arrayType.getNumElements();
#if LLVM_VERSION < 30700
    const DIType arrayTypeInfo = createArrayType(debugBuilder, elementCount, elementTypeInfo);
    const MDNode * const scope = location.getScope(module.getContext());
    const DIVariable arrayVarInfo = createAutoVariable(debugBuilder, DIDescriptor(scope), name, DIFile(scope), 0, arrayTypeInfo);
#else
    DIType * const arrayTypeInfo { createArrayType(debugBuilder, elementCount, elementTypeInfo) };
    DIScope * const scope { location->getScope() };
    DIFile * const file { scope->getFile() };
    DILocalVariable * const arrayVarInfo { createAutoVariable(debugBuilder, scope, name, file, 0, arrayTypeInfo) };
#endif
    insertDeclare(debugBuilder, arrayAllocation, arrayVarInfo, location, &*entryInst);
  }

  // return allocated array to caller for further use
  return arrayAllocation;
}

void csi_inst::attachCSILabelToInstruction(Instruction& inst,
                                           const string& label){
  // based on: http://stackoverflow.com/questions/13425794/adding-metadata-to-instructions-in-llvm-ir
  LLVMContext& C = inst.getContext();
  MDNode* labelMD = MDNode::get(C, MDString::get(C, label));
  inst.setMetadata("CSI.label", labelMD);
}

#if __cplusplus >= 201103L

string csi_inst::to_string(int val) {
  return std::to_string(val);
}

string csi_inst::to_string(unsigned int val) {
  return std::to_string(val);
}

string csi_inst::to_string(long unsigned int val) {
  return std::to_string(val);
}

#else // C++11 or later

namespace csi_inst{
  template<typename T>
  static string to_string(T val){
    ostringstream theStream;
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

string csi_inst::to_string(long unsigned int val){
  return(csi_inst::to_string<long unsigned int>(val));
}

#endif // C++11 or later

string csi_inst::to_string(double val, unsigned int digits){
  ostringstream formatter;
  formatter.precision(digits);
  formatter << val;
  return formatter.str();
}


void createCompileUnit(llvm::DIBuilder &builder, const llvm::Module &module, const llvm::ModulePass &pass)
{
  const PassName passName = pass.getPassName();
#if LLVM_VERSION < 40000
  const string fileName = module.getModuleIdentifier() + '$' + passName;
  builder.createCompileUnit(llvm::dwarf::DW_LANG_C99, fileName, "", passName, false, "", 0);
#else
  const auto fileName = (module.getModuleIdentifier() + '$' + passName).str();
  const auto file = builder.createFile(fileName, "");
  builder.createCompileUnit(llvm::dwarf::DW_LANG_C99, file, passName, false, "", 0);
#endif
}
