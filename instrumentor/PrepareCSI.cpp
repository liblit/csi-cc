//===-------------------------- PrepareCSI.cpp ----------------------------===//
//
// This module pass replicates functions to allow multiple possible
// instrumentation schemes.  Note that, presently, this causes enormous code
// bloat.
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
#define DEBUG_TYPE "csi-prep"

#include "PrepareCSI.h"
#include "InstrumentationData.h"
#include "Utils.hpp"

#include "Versions.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#pragma GCC diagnostic pop

#include "llvm_proxy/CommandLine.h"
#include "llvm_proxy/Module.h"
#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IntrinsicInst.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace csi_inst;
using namespace llvm;
using namespace std;

enum IndirectStyle{
  Std, Ifunc
};
static cl::opt<IndirectStyle> TrampolineStyle("csi-trampoline-style",
   cl::desc("(optional) Trampoline style:"), cl::init(Std),
   cl::values(
     clEnumValN(Std, "std", "(default) switch-based dispatcher function"),
     clEnumValN(Ifunc, "ifunc", "use @gnu_indirect_function attribute "
                                "(requires glibc 2.11.1+, "
                                "binutils 2.20.1+, and an unreleased "
                                "version of LLVM containing r198780)")
     CL_ENUM_VAL_END
   )
);

static cl::opt<bool> SilentInternal("csi-silent", cl::desc("Silence internal "
                                    "warnings.  Will still print errors which "
                                    "cause CSI to fail."));
static cl::opt<string> VariantsFile("csi-variants-file", cl::desc("The path to "
                                   "the instrumentation variants output file."),
                                   cl::value_desc("file_path"));
static cl::opt<bool> NoFilter("csi-no-filter", cl::desc("Do not filter "
                              "instrumentation schemes.  All schemes are used "
                              "verbatim for function replication."));

// Register CSI prep as a pass
char PrepareCSI::ID = 0;
static RegisterPass<PrepareCSI> X("csi",
                "Necessary preparation for any CSI instrumentation",
                false, false);

// Output the bitcode if we want to observe instrumentation changess
#define PRINT_MODULE dbgs() <<                               \
  "\n\n============= MODULE BEGIN ===============\n" << M << \
  "\n============== MODULE END ================\n"

bool PrepareCSI::hasInstrumentationType(const Function &F, const string &type) const {
#if LLVM_VERSION < 30300
  const map<const Function*, set<string> >::const_iterator found =
     functionSchemes.find(&F);
  if (found == functionSchemes.end())
    return false;
  else
    return found->second.count(type);
#else
  return F.hasFnAttribute(type);
#endif
}

void PrepareCSI::addInstrumentationType(Function &F, const string &type) {
#if LLVM_VERSION < 30300
  functionSchemes[&F].insert(type);
#else
  F.addFnAttr(type);
#endif
}

Function* PrepareCSI::switchIndirect(Function* F, GlobalVariable* switcher,
                                     vector<Function*>& replicas){
  F->dropAllReferences();
  
  BasicBlock* newEntry = BasicBlock::Create(*Context, "newEntry", F);
  vector<Value*> callArgs;
  for(Function::arg_iterator k = F->arg_begin(), ke = F->arg_end(); k != ke; ++k)
    callArgs.push_back(&*k);
  
  // set up the switch
  LoadInst* whichCall = new LoadInst(switcher, "chooseCall", true, newEntry);
  SwitchInst* callSwitch = NULL;
  
  // stuff we need
  IntegerType* tInt = Type::getInt32Ty(*Context);

  // create one bb for each possible call (instrumentation scheme)
  bool aZero = false;
  for(unsigned int i = 0; i < replicas.size(); ++i){
    Function* newF = replicas[i];
    BasicBlock* bb = BasicBlock::Create(*Context, "call", F);
    if(callSwitch == NULL){
      callSwitch = SwitchInst::Create(whichCall, bb, replicas.size(),
                                      newEntry);
    }
    string funcName = newF->getName().str();

    if(funcName.length() > 5 &&
       funcName.substr(funcName.length()-5, 5) == "$none"){
      callSwitch->addCase(ConstantInt::get(tInt, 0), bb);
      if(aZero)
        report_fatal_error("multiple defaults for function '" +
                           F->getName() + "'");
      aZero = true;
    }
    else
      callSwitch->addCase(ConstantInt::get(tInt, i+1), bb);

    CallInst* oneCall = CallInst::Create(newF, callArgs,
       (F->getReturnType()->isVoidTy()) ? "" : "theCall", bb);
    oneCall->setTailCall(true);
    if(F->getReturnType()->isVoidTy())
      ReturnInst::Create(*Context, bb);
    else
      ReturnInst::Create(*Context, oneCall, bb);
  }
  // note that we intentionally started numbering the cases from 1 so that the
  // zero case is reserved for the uninstrumented variant (if there is one)
  if(!aZero)
    switcher->setInitializer(ConstantInt::get(tInt, 1));

  return(F);
}

Function* PrepareCSI::ifuncIndirect(Function* F, GlobalVariable* switcher,
                                    vector<Function*>& replicas){
  F->dropAllReferences();
  
  ArrayRef<Type*> newParams;
  
  Function* newF = Function::Create(
     FunctionType::get(F->getFunctionType()->getPointerTo(), newParams, false),
     F->getLinkage(), F->getName(), F->getParent());
  
  // fix up all calls to this function in the module.  They would otherwise be
  // broken references to the old F.  While this bitcast looks like a no-op,
  // it will actually end up making all in-module calls to F look like indirect
  // calls, which has the result of making everything work
  Constant* replacement = ConstantExpr::getCast(Instruction::BitCast, newF,
                             F->getFunctionType()->getPointerTo());
  F->replaceAllUsesWith(replacement);

  newF->takeName(F);
  newF->getParent()->appendModuleInlineAsm(".type " + newF->getName().str() +
                                           ", @gnu_indirect_function\x0A");
  F->eraseFromParent();
  
  // set up the switch
  BasicBlock* newEntry = BasicBlock::Create(*Context, "entry", newF);
  LoadInst* whichFn = new LoadInst(switcher, "chooseFn", true, newEntry);
  SwitchInst* callSwitch = NULL;
  
  // stuff we need
  IntegerType* tInt = Type::getInt32Ty(*Context);
  
  // create one bb for each possible call (instrumentation scheme)
  bool aZero = false;
  for(unsigned int i = 0; i < replicas.size(); ++i){
    Function* selectF = replicas[i];
    BasicBlock* bb = BasicBlock::Create(*Context, "call", newF);
    if(callSwitch == NULL){
      callSwitch = SwitchInst::Create(whichFn, bb, replicas.size(),
                                      newEntry);
    }
    string funcName = selectF->getName().str();
    if(funcName.length() > 5 &&
       funcName.substr(funcName.length()-5, 5) == "$none"){
      callSwitch->addCase(ConstantInt::get(tInt, 0), bb);
      aZero = true;
    }
    else
      callSwitch->addCase(ConstantInt::get(tInt, i+1), bb);
    ReturnInst::Create(*Context, selectF, bb);
  }
  // note that we intentionally started numbering the cases from 1 so that the
  // zero case is reserved for the uninstrumented variant (if there is one)
  if(!aZero)
    switcher->setInitializer(ConstantInt::get(tInt, 1));
  
  return(newF);
}

void printScheme(vector<pair<string, set<set<string> > > >& schemeData){
  dbgs() << "------Scheme------\n";
  for(vector<pair<string, set<set<string> > > >::iterator i = schemeData.begin(), e = schemeData.end(); i != e; ++i){
    pair<string, set<set<string> > > entry = *i;
    dbgs() << entry.first << ": ";
    for(set<set<string> >::iterator j = entry.second.begin(), je = entry.second.end(); j != je; ++j){
      set<string> jentry = *j;
      if(j != entry.second.begin())
        dbgs() << ',';
      dbgs() << '{';
      for(set<string>::iterator k = jentry.begin(), ke = jentry.end(); k != ke; ++k){
        if(k != jentry.begin())
          dbgs() << ',';
        dbgs() << *k;
      }
      dbgs() << '}';
    }
    dbgs() << '\n';
  }
  dbgs() << "------------------\n";
}

static vector<string> split(string s, char delim){
  vector<string> result;
  
  stringstream lineStream(s);
  string entry;
  while(getline(lineStream, entry, delim)){
    result.push_back(entry);
    if(lineStream.fail() || lineStream.eof())
      break;
  }
  if(lineStream.bad())
    report_fatal_error("internal error encountered parsing scheme input");
  
  return(result);
}

static bool patternMatch(const string &text, const string &pattern){
  return pattern == text || pattern == "*";
}

static void verifyScheme(const vector<pair<string, set<set<string> > > >& scheme) {
  for(vector<pair<string, set<set<string> > > >::const_iterator i = scheme.begin(), e = scheme.end(); i != e; ++i){
    for(set<set<string> >::const_iterator j = i->second.begin(), je = i->second.end(); j != je; ++j){
      for(set<string>::const_iterator k = j->begin(), ke = j->end(); k != ke; ++k){
        if(Instrumentors.count(*k) < 1)
          report_fatal_error("invalid instrumentor '" + *k + "' in scheme");
      }
    }
  }
}

static vector<pair<string, set<set<string> > > > readScheme(istream& in){
  vector<string> lines;
  string s;
  while(getline(in, s)){
    s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
    if(!s.empty())
      lines.push_back(s);
    if(in.fail() || in.eof())
      break;
  }
  if(in.bad())
    report_fatal_error("error encountered reading schema input");
  
  vector<pair<string, set<set<string> > > > result;
  
  for(vector<string>::iterator i = lines.begin(), e = lines.end(); i != e; ++i){
    vector<string> entries = split(*i, ';');
    if(entries.size() < 2)
      report_fatal_error("invalid formatting for line '" + *i + "' in instrumentation schema");
    
    string fnPattern = entries[0];
    set<set<string> > schemes;
    for(vector<string>::iterator j = ++entries.begin(), je = entries.end(); j != je; ++j){
      string scheme = *j;
      transform(scheme.begin(), scheme.end(), scheme.begin(), ::toupper);
      if(scheme[0] != '{' || scheme[scheme.length()-1] != '}')
        report_fatal_error("invalid formatting for entry '" + scheme + "' in instrumentation schema", false);
      scheme.erase(0, 1);
      scheme.erase(scheme.length()-1, 1);
      
      const vector<string> methods = split(scheme, ',');
      set<string> methodsSet;
      for(vector<string>::const_iterator k = methods.begin(), ke = methods.end(); k != ke; ++k){
        if(!k->empty())
          methodsSet.insert(*k);
      }
      
      schemes.insert(methodsSet);
    }
    
    result.push_back(make_pair(fnPattern, schemes));
  }
  
  return(result);
}

static void labelInstructions(Function& F){
  unsigned int label = 1;
  for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
    attachCSILabelToInstruction(*i, csi_inst::to_string(label++));
}

// Entry point of the module
bool PrepareCSI::runOnModule(Module &M){
  // first, label all instructions in all functions in the module (so later
  // passes can have a unique identifier for each instruction)
  for(Module::iterator F = M.begin(), e = M.end(); F != e; ++F)
    labelInstructions(*F);

  // then, proceed with reading in scheme data
  vector<pair<string, set<set<string> > > > schemeData;
  if(VariantsFile.empty()){
    outs() << "Reading stdin for instrumentation scheme...\n";
    schemeData = readScheme(cin);
    outs() << "Finished reading stdin for scheme\n";
  }
  else{
    ifstream inFile(VariantsFile.c_str(), ios::in);
    if(!inFile || !inFile.is_open())
      report_fatal_error("cannot open specified instrumentation scheme file: " +
                         VariantsFile);
    schemeData = readScheme(inFile);
  }
  
  DEBUG(printScheme(schemeData));
  
  // verify that all passes provided exist
  DEBUG(dbgs() << "verifying...\n");
  verifyScheme(schemeData);
  
  DEBUG(printScheme(schemeData));
  
  Context = &M.getContext();
  
  // Find the matching pattern for each function
  map<Function*, set<set<string> > > matches;
  for(Module::iterator F = M.begin(), E = M.end(); F != E; ++F){
    if(F->isDeclaration() || F->isIntrinsic())
      continue;
    bool found = false;
    for(vector<pair<string, set<set<string> > > >::iterator i = schemeData.begin(), e = schemeData.end(); i != e; ++i){
      if(patternMatch(F->getName(), i->first)){
        matches[&*F] = i->second;
        found = true;
        break;
      }
    }
    if(!found){
      errs() << "WARNING: No scheme match found for function '"
             << F->getName() << "'.  Skipping.\n";
      continue;
    }
  }

  // Filter patterns matched to each function, and replicate
  for(map<Function*, set<set<string> > >::iterator i = matches.begin(), e = matches.end(); i != e; ++i){
    Function* F = i->first;
    
    // go through all filters for each possible scheme.  after passing through
    // all filters, make a replica of this function with those tags on it,
    // and add that replica to the "replicas" set.  else, print warning
    set<set<string> > replicas;
    
    for(set<set<string> >::iterator j = i->second.begin(), je = i->second.end(); j != je; ++j){
      set<string> testing = *j;

      bool passed = true;
      if(!NoFilter){
        for(vector<FilterFn>::const_iterator fi = Filters.begin(), fe = Filters.end(); fi != fe; ++fi){
          if((*fi)(testing, F))
            passed = false;
        }
      }
      replicas.insert(testing);
      if(!passed){
        outs() << "WARNING: filtered scheme '";
        for(set<string>::iterator k = j->begin(), ke = j->end(); k != ke; ++k){
          if(k != j->begin())
            outs() << ',';
          outs() << *k;
        }
        outs() << "' for function '" << F->getName().str() << "'\n";
        continue;
      }
    }
    
    switch (replicas.size()) {
    case 0:
      continue;
    case 1: {
      // instrument the original body (don't replicate and trampoline)
      const set<string>& scheme = *(replicas.begin());
      for(set<string>::iterator j = scheme.begin(), je = scheme.end(); j != je; ++j)
        addInstrumentationType(*F, *j);
      break;
    }
    default:
      // if the function is variable-argument, currently don't support
      if(F->isVarArg()){
        outs() << "WARNING: cannot instrument variable-argument function '"
               << F->getName() << "'\n";
        continue;
      }
      
      // make a function for each scheme
      vector<Function*> funcReplicas;
      for(set<set<string> >::iterator j = replicas.begin(), je = replicas.end(); j != je; ++j){
        ValueToValueMapTy valueMap;
        SmallVector<ReturnInst*, 1> returns;
        Function* newF = CloneFunction(F, valueMap,
#if LLVM_VERSION < 30900
                                       false,
#endif
                                       NULL);

        string name = F->getName().str();
        if(j->begin() == j->end())
          name += "$none";
        for(set<string>::iterator k = j->begin(), ke = j->end(); k != ke; ++k){
          name += '$' + *k;
          addInstrumentationType(*newF, *k);
        }
        newF->setName(name);
        
        // NOTE: this does not preserve function ordering, thus it could
        // randomly slightly impact performance positively or negatively
        F->getParent()->getFunctionList().push_back(newF);
        funcReplicas.push_back(newF);
      }
      
      // assign this function a global switcher variable
      IntegerType* tInt = Type::getInt32Ty(*Context);
      string globalName = "__CSI_inst_"+getUniqueCFunctionName(*F);
      const GlobalValue::LinkageTypes linkage =
         F->hasAvailableExternallyLinkage()
         ? GlobalValue::WeakAnyLinkage
         : GlobalValue::ExternalLinkage;
      GlobalVariable * const functionGlobal =
         new GlobalVariable(M, tInt, true, linkage,
                            ConstantInt::get(tInt, 0), globalName);
      
      functionGlobal->setSection("__CSI_func_inst");

      // set up the trampoline call for this function
      switch(TrampolineStyle){
        case Ifunc: if(F->getLinkage() != GlobalValue::InternalLinkage){
                      // we may be able to change linkage on static functions so
                      // they can be used with ifunc, but this has the potential
                      // for name collisions, and the performance impact should
                      // be negligible in any case
                      ifuncIndirect(F, functionGlobal, funcReplicas);
                      break;
                    }
                    // intentional fallthrough
        case Std:   switchIndirect(F, functionGlobal, funcReplicas); break;
        default:    llvm_unreachable_internal("bad indirect function style value");
      }
    }
  }
  
  return(true);
}
