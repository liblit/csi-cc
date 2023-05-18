//===--------------------- CoverageOptimization.cpp -----------------------===//
//
// This pass gathers information for coverage optimization, and
// exposes an interface for callers to ask for customized, optimized coverage.
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
#define DEBUG_TYPE "coverage-optimization"

#ifdef USE_GAMS
#include "GAMSUtils.h"
#endif

#ifdef USE_LEMON
#include "LEMONUtils.h"
#endif

#include "CoverageOptimization.h"
#include "NaiveCoverageSet.h"
#include "NaiveOptimizationGraph.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>

#include "llvm_proxy/CommandLine.h"
#include "llvm_proxy/Instructions.h"

#include <iostream>
#include <queue>
#include <stack>

using namespace csi_inst;
using namespace llvm;
using namespace std;


// option for optimization data construction
static cl::opt<bool> IncompleteExe("opt-incomplete-exe", cl::desc("Optimize "
                                   "for incomplete executions.  Presently, "
                                   "this means that the crash set (X) is "
                                   "every node in the CFG."));

// option to always verify that optimization result is a coverage set
static cl::opt<bool> VerifyCoverageSet("opt-verify-coverage-set", cl::desc(
                                       "Verify that the optimization result "
                                       "is a coverage set of desired data."));

// option to always verify that optimization result is sufficiently optimal
static cl::opt<bool> VerifyOptimality("opt-verify-optimality",
                                      cl::desc("Verify that the optimization "
                                               "result is sufficiently optimal "
                                               "based on requested optimality "
                                               "level."));

// option to log stats on optimization result
static cl::opt<bool> LogStats("opt-log-stats",
			      cl::desc("Log stats on coverage set cost and "
				       "size for returned optimal result."));

// option for approximation optimization (level o2) style
enum ApproxStyle {
  DOMINATORS, LOCAL, LOCAL_WITH_PREPASS
};
static cl::opt<ApproxStyle> ApproximationStyle("opt-approx-style",
                               cl::desc("Approximation style to use when "
                                        "running coverage optimization level "
                                        "o2 (local optimum)"),
                               cl::init(LOCAL_WITH_PREPASS),
                               cl::values(
                                 clEnumValN(DOMINATORS, "simple",
                                            "simple approximation"),
                                 clEnumValN(LOCAL, "local",
                                            "(default) basic locally-optimal"),
                                 clEnumValN(LOCAL_WITH_PREPASS, "local-prepass",
                                            "simple as prepass, then local")
                                 CL_ENUM_VAL_END
                               )
                            );

// option for full optimization (level o3) style
enum FullOptStyle {
  GAMS_STYLE, LEMON_STYLE
};
static cl::opt<FullOptStyle> FullyOptimalStyle("opt-full-style",
                               cl::desc("Optimization style to use when "
                                        "running coverage optimization level "
                                        "o3 (global optimum)"),
#ifdef USE_GAMS
                               cl::init(GAMS_STYLE),
#else
                               cl::init(LEMON_STYLE),
#endif
                               cl::values(
                                 clEnumValN(GAMS_STYLE, "gams",
                                            "(default) fully enumerative style "
                                            "based on Farkas's lemma in GAMS"),
                                 clEnumValN(LEMON_STYLE, "lemon",
                                            "iterative solve over LEMON "
                                            "graphs")
                                 CL_ENUM_VAL_END
                               )
                            );


#ifdef USE_GAMS
static cl::opt<string> InstallGamsDir("opt-gams-install-dir",
                                  cl::desc("The directory containing the gams "
                                           "executable."),
                                  cl::value_desc("directory"));

static cl::opt<string> RunGamsDir("opt-gams-run-dir",
                                  cl::desc("The directory to use as CWD when "
                                           "running the GAMS executable."),
                                  cl::value_desc("directory"));

static cl::opt<string> GmsFile("opt-gams-file",
                               cl::desc("The path to the GAMS file (it came "
                                        "with your csi-cc installation, and "
                                        "usually has a .gms extension)."),
                               cl::value_desc("file_path"));

static cl::opt<string> GdxFile("opt-gdx-file",
                               cl::desc("The path to the generated .gdx file "
                                        "(i.e. the generated input data for "
                                        "the optimization problem given in "
                                        "sg-gams-file)."),
                               cl::value_desc("file_path"));

static cl::opt<string> ResultGdxFile("opt-result-gdx-file",
                               cl::desc("The path to the output .gdx file "
                                        "(i.e. the result of running "
                                        "sg-gams-file with sg-gdx-file as "
                                        "input)."),
                               cl::value_desc("file_path"));

static cl::opt<string> LogFile("opt-gams-log-file",
                               cl::desc("The path to the log file generated by "
                                        "the call to the GAMS framework."),
                               cl::value_desc("file_path"));
#endif

// Register CoverageOptimizationData as a pass
char CoverageOptimizationData::ID = 0;
static RegisterPass<CoverageOptimizationData> X("coverageOpt",
                "Analysis pass to precompute coverage optimization data",
                true, true);

#if defined(USE_GAMS) || defined(USE_LEMON)
set<BasicBlock*> CoverageOptimizationData::getOptimizedProbes_full(
                          set<BasicBlock*>* canProbe,
                          set<BasicBlock*>* wantData,
                          set<BasicBlock*>* crashPoints) const {
  set<BasicBlock*> optimalResult;
  switch(FullyOptimalStyle){
#ifdef USE_GAMS
    case GAMS_STYLE: {
      // TODO: make sure GmsFile, GdxFile, ResultGdxFile, LogFile, RunGamsDir
      // are valid
      GAMSinterface Ginter(InstallGamsDir);
      optimalResult = Ginter.optimizeModel(GmsFile, GdxFile,
                                           ResultGdxFile, LogFile,
                                           RunGamsDir, *graph,
                                           canProbe, wantData,
                                           crashPoints);
      break;
    }
#endif
#ifdef USE_LEMON
    case LEMON_STYLE: {
      LEMONsolver solver(*graph);
      optimalResult = solver.optimize(canProbe, wantData,
                                      crashPoints, LogStats);
      break;
    }
#endif
    default:
      report_fatal_error("Invalid optimization style chosen");
  }

  if(VerifyOptimality){
    set<BasicBlock*> resultAfterLocal = graph->getOptimizedProbes(
                                           tree.getOptimizedProbes(
                                              optimalResult,
                                              *wantData,
                                              *crashPoints),
                                           *wantData,
                                           *crashPoints);
    if(resultAfterLocal != optimalResult){
      report_fatal_error("Returned optimized result: " +
                         setBB_asstring(optimalResult) +
                         "\nis less optimal than a locally-optimal solution: " +
                         setBB_asstring(resultAfterLocal));
    }
  }

  return(optimalResult);
}
#endif

set<BasicBlock*> CoverageOptimizationData::getOptimizedProbes_cheap(
                          set<BasicBlock*>* canProbe,
                          set<BasicBlock*>* wantData,
                          set<BasicBlock*>* crashPoints) const {
  // get the approximate result (using the appropriate structure)
  set<BasicBlock*> approxResult;
  switch(ApproximationStyle){
    case DOMINATORS:
      approxResult = tree.getOptimizedProbes(*canProbe,
                                             *wantData,
                                             *crashPoints);
      break;
    case LOCAL:
      approxResult = graph->getOptimizedProbes(*canProbe,
                                               *wantData,
                                               *crashPoints);
      break;
    case LOCAL_WITH_PREPASS:
      approxResult = graph->getOptimizedProbes(
                        tree.getOptimizedProbes(*canProbe,
                                                *wantData,
                                                *crashPoints),
                        *wantData,
                        *crashPoints);
      break;
    default:
      report_fatal_error("Invalid approximation style chosen");
  }

  return(approxResult);
}


double getCostOfSet(const set<BasicBlock*>& coverageSet,
		    const CoverageOptimizationGraph& graph){
  double setCost = 0.0;
  for(set<BasicBlock*>::const_iterator i = coverageSet.begin(),
	                               e = coverageSet.end();
      i != e; ++i)
    setCost += graph.getBlockCost(*i);
  return(setCost);
}

set<BasicBlock*> CoverageOptimizationData::getOptimizedProbes(Function* F,
                set<BasicBlock*>* canProbe,
                set<BasicBlock*>* wantData
#if defined(USE_GAMS) || defined(USE_LEMON)
                , bool fullOptimization
#endif
                ) const {
  DEBUG(dbgs() << "Optimizing function: " << F->getName().str() << '\n');

  // TODO: can we get rid of this again?
  set<BasicBlock*> fullCan;
  if(canProbe == NULL){
    for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i)
      fullCan.insert(&*i);
    canProbe = &fullCan;
  }
  set<BasicBlock*> fullWant;
  if(wantData == NULL){
    for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
      // oddball fix: calls to "exit()" sometimes result in unreachable blocks
      if(pred_begin(&*i) != pred_end(&*i) || &*i == &F->getEntryBlock())
        fullWant.insert(&*i);
    }
    wantData = &fullWant;
  }

  // get all possible stopping basic blocks
  set<BasicBlock*> crashPoints;
  if(IncompleteExe){
    for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i)
      crashPoints.insert(&*i);
  }
  else{
    for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
      if(succ_begin(&*i) == succ_end(&*i)){
        TerminatorInst* terminator = i->getTerminator();
        if(dyn_cast<ReturnInst>(terminator))
          crashPoints.insert(&*i);
        else if(!dyn_cast<UnreachableInst>(terminator))
          report_fatal_error("Coverage optimization encountered terminal "
                             "block that it didn't know how to handle");
      }
    }
  }

  set<BasicBlock*> result;
#if defined(USE_GAMS) || defined(USE_LEMON)
  if(fullOptimization)
    result = getOptimizedProbes_full(canProbe, wantData, &crashPoints);
  else
#endif
    result = getOptimizedProbes_cheap(canProbe, wantData, &crashPoints);



  if(LogStats){

    dbgs() << "Optimized result: " << setBB_asstring(result) << '\n';
    dbgs() << "Optimized result cost: " << to_string(getCostOfSet(result, *graph)) << '\n';

    string fileName = "";
    string fileDir = "";
#if LLVM_VERSION > 30700
    DISubprogram* functionMD = F->getSubprogram();
    if(functionMD != NULL){
      fileName = functionMD->getFilename().str();
      fileDir = functionMD->getDirectory().str();
    }
    replace(fileName.begin(), fileName.end(), ',', '_');
    replace(fileDir.begin(), fileDir.end(), ',', '_');
#endif
    dbgs() << "EKK2000: " << F->getName().str() << ','
         << fileName << ',' << fileDir << ',' << result.size()
         << ',' << to_string(getCostOfSet(result, *graph)) << "\n";
  }

  if(VerifyCoverageSet){
    // use the naive coverage set check to verify the optimized result
    if(!isCoverageSet(result, *wantData,
                      graph->getEntryBlock(), crashPoints))
      report_fatal_error("Returned optimized result: " +
                         setBB_asstring(result) +
                         "\nis not a coverage set for desired: " +
                         setBB_asstring(*wantData));
  }

  return(result);
}

#if LLVM_VERSION < 30800
namespace llvm {
  typedef BlockFrequencyInfo BlockFrequencyInfoWrapperPass;
}
#endif

bool CoverageOptimizationData::runOnFunction(Function& F){
  BlockFrequencyInfoWrapperPass& bfPass = getAnalysis<BlockFrequencyInfoWrapperPass>();
#if LLVM_VERSION < 30800
  BlockFrequencyInfo& bf = bfPass;
#else
  BlockFrequencyInfo& bf = bfPass.getBFI();
#endif
  DominatorTree& domTree = getDominatorTree(*this);
  this->graph.reset(new NaiveOptimizationGraph(&F, bf));
  this->tree = DominatorOptimizationGraph(&F, bf, domTree);
  return(false);
}

// We currently need BlockFrequencyInfo and dominators to run coverage
// optimization
void CoverageOptimizationData::getAnalysisUsage(AnalysisUsage& AU) const {
  AU.setPreservesAll();
  AU.addRequired<BlockFrequencyInfoWrapperPass>();
  addRequiredDominatorTree(AU);
}
