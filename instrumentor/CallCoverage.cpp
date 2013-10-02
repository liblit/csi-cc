//===------------------------- CallCoverage.cpp ---------------------------===//
//
// This pass instruments function calls for interprocedural analysis by
// gathering both global and local coverage information.
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
#define DEBUG_TYPE "call-coverage"

#include "CallCoverage.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#include "llvm_proxy/IntrinsicInst.h"
#include "llvm_proxy/Module.h"

#include <iostream>

using namespace csi_inst;
using namespace llvm;
using namespace std;

static cl::opt<bool> SilentInternal("cc-silent", cl::desc("Silence internal "
                                    "warnings.  Will still print errors which "
                                    "cause CC to fail."));
static cl::opt<string> InfoFile("cc-info-file", cl::desc("The path to "
                                "the call coverage info file."),
                                cl::value_desc("file_path"));

// Register call coverage as a pass
char CallCoverage::ID = 0;
static RegisterPass<CallCoverage> X("call-coverage",
                "Insert call coverage instrumentation",
                false, false);

void CallCoverage::writeFunctionValue(Function& F){
  _infoStream << "#" << F.getName().str() << "|"
              << funcToGlobalArray[&F]->getName().str() << endl;
}

void CallCoverage::writeOneCall(CallInst* theCall, unsigned int index){
  unsigned int lineNum = 0;
  DebugLoc dbLoc = theCall->getDebugLoc();
  if(!dbLoc.isUnknown())
    lineNum = dbLoc.getLine();
  string fnName = "?";
  Function* calledFn = theCall->getCalledFunction();
  if(calledFn)
    fnName = calledFn->getName();
  _infoStream << index << "|" << lineNum << "|" << fnName << endl;
}

// ***
// runOnFunction: Entry point of the function
// ***
bool CallCoverage::runOnFunction(Function& F) {
  DEBUG(dbgs() << "Function: " << F.getName() << "\n");
  
  set<CallInst*> myCalls = functionCalls[&F];
  unsigned int arraySize = myCalls.size();
  
  PathTracing* pathsPass = getAnalysisIfAvailable<PathTracing>();
  map<Instruction*, Instruction*> oldToNewMap;
  if(pathsPass)
    oldToNewMap = pathsPass->oldToNewCallMap;
  else{
    for (set<CallInst*>::iterator i = myCalls.begin(), e = myCalls.end(); i != e; ++i){
      oldToNewMap[*i] = NULL;
    }
  }
  
  if(arraySize != oldToNewMap.size()){
    cerr << "ERROR: Call-site count mismatch for Path and Coverage passes "
         << "for instrumented function '" + F.getName().str() + "'.  Values "
         << "were " << arraySize << " and " << oldToNewMap.size() << ".  This "
         << "is a tool error." << endl;
    exit(1);
  }
  else if(arraySize < 1)
    return false;
  
  Type* tBool = Type::getInt8Ty(*Context);
  Type* tInt = Type::getInt32Ty(*Context);
  Type* tArr = ArrayType::get(tBool, arraySize);
  
  // grab the global coverage array
  GlobalVariable* arrGlobal = funcToGlobalArray[&F];
  
  // declare the local coverage array
  Instruction* entryInst = F.getEntryBlock().getFirstNonPHI();
  AllocaInst* arrInst = new AllocaInst(tArr, "__CC_arr", entryInst);
  std::vector<Value*> gepIndices(2);
  gepIndices[0] = Constant::getNullValue(Type::getInt32Ty(*Context));
  for(unsigned int i = 0; i < arraySize; i++){
    gepIndices[1] = ConstantInt::get(tInt, i);
    GetElementPtrInst* anEntry = GetElementPtrInst::Create(arrInst, gepIndices,
                                    "initArray", entryInst);
    new StoreInst(ConstantInt::get(tBool, 0), anEntry, true, entryInst);
  }
  
  // get the debug location of any instruction in the entry
  // block--this will use the same info.  If there is none,
  // technically we should build it, but that's a pain in
  // the ass (if it's possible) so I just give up right now
  BasicBlock* entryBB = F.getEntryBlock().getFirstNonPHI()->getParent();
  DebugLoc dbLoc;
  for(BasicBlock::iterator i = entryBB->begin(), e = entryBB->end(); i != e; ++i){
    dbLoc = i->getDebugLoc();
    if(!dbLoc.isUnknown())
      break;
  }
  if(dbLoc.isUnknown()){
    // try again iterating through the entire function...
    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i){
      dbLoc = i->getDebugLoc();
      if(!dbLoc.isUnknown())
        break;
    }
    
    if(dbLoc.isUnknown() && !SilentInternal){
      errs() << "WARNING: there will be no debug locations for instrumented"
             << " function "+F.getName()+" . Call coverage will likely "
             << "be unextractable!\n";
    }
    else if(!SilentInternal){
      DEBUG(dbgs() << "WARNING: debug location outside of entry block used "
                   << "for instrumented function "+F.getName()+"\n");
    }
  }
  
  // give it debug info
  if(!dbLoc.isUnknown()){
    DIType boolType = Builder->createBasicType("__cc_bool", 8, 8, dwarf::DW_ATE_boolean);
    Value* subscript = Builder->getOrCreateSubrange(0, arraySize-1);
    DIArray subscriptArray = Builder->getOrCreateArray(subscript);
    DIType arrType = Builder->createArrayType(arraySize*8, 8, boolType, 
                                              subscriptArray);
    DIVariable arrDI = Builder->createLocalVariable(
                        (unsigned)dwarf::DW_TAG_auto_variable,
                        DIDescriptor(dbLoc.getScope(*Context)),
                        "__CC_arr",
                        DIFile(dbLoc.getScope(*Context)), 0, arrType);
    Instruction* declareArr = Builder->insertDeclare(arrInst, arrDI, entryInst);
    declareArr->setDebugLoc(dbLoc);
  }
  
  // write out the function name and its arrays
  writeFunctionValue(F);
  
  // instrument each call instruction
  unsigned int curIdx = 0;
  for(map<Instruction*, Instruction*>::iterator i = oldToNewMap.begin(), e = oldToNewMap.end(); i != e; ++i){
    CallInst* oldCall = dyn_cast<CallInst>(i->first);
    if(!oldCall || (oldCall->getCalledFunction() &&
                    oldCall->getCalledFunction()->isIntrinsic()))
      continue;
    // if the body was not replicated, second will be NULL
    CallInst* newCall = (i->second == NULL) ?
                           NULL : dyn_cast<CallInst>(i->second);
        
    if(!myCalls.count(oldCall) || (newCall != NULL && !myCalls.count(newCall))){
      cerr << "ERROR: set of calls does not match for instrumentation passes "
           << "for function " << F.getName().str() << endl;
      exit(1);
    }
    
    Instruction* afterOld = nextInst(oldCall);
    gepIndices[0] = Constant::getNullValue(Type::getInt32Ty(*Context));
    gepIndices[1] = ConstantInt::get(tInt, curIdx);
    GetElementPtrInst* oldGEP = GetElementPtrInst::Create(arrInst, gepIndices,
                                   "localCall", afterOld);
    new StoreInst(ConstantInt::get(tBool, 1), oldGEP, true, afterOld);
    GetElementPtrInst* oldGGEP = GetElementPtrInst::Create(arrGlobal, gepIndices,
                                   "globalCall", afterOld);
    new StoreInst(ConstantInt::get(tBool, 1), oldGGEP, true, afterOld);
    
    // there will be no second call if the function body was not replicated
    if(newCall){
      Instruction* afterNew = nextInst(newCall);
      GetElementPtrInst* newGEP = GetElementPtrInst::Create(arrInst, gepIndices,
                                     "localCall", afterNew);
      new StoreInst(ConstantInt::get(tBool, 1), newGEP, true, afterNew);
      GetElementPtrInst* newGGEP = GetElementPtrInst::Create(arrGlobal,
                                      gepIndices, "globalCall", afterNew);
      new StoreInst(ConstantInt::get(tBool, 1), newGGEP, true, afterNew);
    }
    
    // write out the call's function, line number, and index
    writeOneCall(oldCall, curIdx);
    
    curIdx++;
  }
  
  return true;
}

// ***
// doInitialization: Insert the necessary global variable before
// instrumenting each function.
// ***
bool CallCoverage::doInitialization(Module& M){
  if(InfoFile == ""){
    cerr << "ERROR: CC cannot continue.  -cc-info-file [file] is required.\n";
    exit(1);
  }
  _infoStream.open(InfoFile.c_str(), ios::out | ios::trunc);
  if(!_infoStream.is_open()){
    cerr << "ERROR: unable to open cc-file location: " << InfoFile << endl;
    exit(2);
  }
  else
    DEBUG(string("Output stream opened to ") + InfoFile.c_str());
  
  Context = &M.getContext();
  Builder = new DIBuilder(M);
  
  // find all call nodes in each function in the module (before replicating
  // the body) so it knows how big to make the global (and local) array for
  // each function
  functionCalls.clear();
  for(Module::iterator F = M.begin(), e = M.end(); F != e; ++F){
    if(F->isDeclaration() || F->isIntrinsic() ||
       F->getName().substr(0, 5).equals("__PT_"))
      continue;
    
    //unsigned int funcCounter = 0;
    set<CallInst*> fCalls;
    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i){
      Instruction* inst = &(*i);
      
      if(CallInst* castInst = dyn_cast<CallInst>(inst)){
        // if we can't get the called function it very likely indicates
        // a function pointer, so we should still instrument
        if(castInst->getCalledFunction() &&
           castInst->getCalledFunction()->isIntrinsic()){
          // don't waste time for intrinsics.
          // NOTE: could also exclude other uninteresting functions here
          continue;
        }
        
        fCalls.insert(castInst);
      }
    }
    
    //functionArraySize[F] = funcCounter;
    functionCalls[F] = fCalls;
  }
  
  funcToGlobalArray.clear();
  
  // the type of bool
  Type* tInt = Type::getInt8Ty(*Context);
  
  // some debug info preliminaries
  DIBuilder* globalBuilder = new DIBuilder(M);
  globalBuilder->createCompileUnit(dwarf::DW_LANG_C99, M.getModuleIdentifier(),
                                   "", getPassName(), false, "", 0);
  DIType boolType = globalBuilder->createBasicType("__cc_bool", 8, 8,
                                                   dwarf::DW_ATE_boolean);
  
  // loop to make globals for each function
  for(Module::iterator F = M.begin(), e = M.end(); F != e; ++F){
    unsigned int arraySize = functionCalls[F].size();
    if(arraySize < 1)
      continue;
    Type* tArr = ArrayType::get(tInt, arraySize);
    GlobalVariable* theGlobal = new GlobalVariable(M, tArr, false,
                                       GlobalValue::ExternalLinkage,
                                       Constant::getNullValue(tArr),
                                       "__CC_arr_" + F->getName().str());
    funcToGlobalArray[F] = theGlobal;
    
    // create type debug info for new variables
    Value* subscript = globalBuilder->getOrCreateSubrange(0, arraySize-1);
    DIArray subscriptArray = globalBuilder->getOrCreateArray(subscript);
    DIType arrType = globalBuilder->createArrayType(arraySize*8, 8, boolType, 
                                                    subscriptArray);
    globalBuilder->createGlobalVariable(theGlobal->getName(), DIFile(),
                                        0, arrType, false, theGlobal);
  }
  
  globalBuilder->finalize();
  return true;
}

// ***
// doFinalization: Close the stream for the info file writing.
// ***
bool CallCoverage::doFinalization(Module& M){
  (void)M; // suppress warning
  
  if(_infoStream.is_open())
    _infoStream.close();
  return false;
}
