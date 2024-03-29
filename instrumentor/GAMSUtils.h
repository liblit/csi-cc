//===---------------------------- GAMSUtils.h -----------------------------===//
//
// Utilities for interfacing with the GAMS optimization framework.
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
#ifndef CSI_GAMS_UTILS_H
#define CSI_GAMS_UTILS_H

#include "CoverageOptimizationGraph.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <gamsxco.hpp>
#include <gdxco.hpp>
#include <optco.hpp>
#pragma GCC diagnostic pop

#include <llvm/Analysis/BlockFrequencyInfo.h>

#include <set>
#include <map>

namespace csi_inst {

class GAMSinterface{
private:
  // the GAMS objects
    // GAMS execution object
  GAMS::GAMSX gamsx;
    // GAMS data exchange object
  GAMS::GDX gdx;
    // GAMS options object
  GAMS::OPT opt;
  
  // the GAMS system directory
  const std::string sysdir;
  
  // a mapping from block names to BasicBlock pointers (filled by
  // writeModelData() on each call to optimizeModel())
  std::map<std::string, llvm::BasicBlock*> blockNameMap;
  // a mapping from BasicBlock pointers to block names (filled by
  // writeModelData() on each call to optimizeModel())
  std::map<llvm::BasicBlock*, std::string> nameBlockMap;
  
  
  // load objects from the GAMS system directory
  void loadObjects();
  
  // write out sets/parameters into the current gdx file
  void writeSet(std::string name, std::string desc, std::set<std::string> data);
  void writeSetOfPair(std::string name, std::string desc,
                      std::set<std::pair<std::string, std::string> > data);
  void write2dSet(std::string name, std::string desc,
                  std::map<std::string, std::set<std::string> > data);
  void write4dSet(std::string name, std::string desc,
     std::map<std::string,
              std::map<std::string,
                       std::map<std::string, std::set<std::string> > > >& data);
  void writeParameter(std::string name, std::string desc,
                      std::map<std::string, double> data);
  
  void callGams(const std::string gamsFile, const std::string resultFile,
                const std::string logFile, const std::string runDir);
  
  // write out necessary model data to file gdxFile
  // (gdxFile will be overwritten or created)
  void writeModelData(const std::string gdxFile,
     const csi_inst::CoverageOptimizationGraph& graph,
     const std::set<llvm::BasicBlock*>* canProbe,
     const std::set<llvm::BasicBlock*>* wantData,
     const std::set<llvm::BasicBlock*>* crashPoints);
  
  
  // start/finish reading parameter "varName" (of dimension "dimension") from
  // the current open gdx file
  void startParameterRead(std::string varName, int dimension);
  void finishParameterRead(std::string varName);
  
  // read the solution data generated by the call to GAMS
  std::set<llvm::BasicBlock*> readSolutionData(const std::string resultFile);

public:
  // constructor
  GAMSinterface(const std::string gamsdir);
  
  std::set<llvm::BasicBlock*> optimizeModel(const std::string gamsFile,
     const std::string gdxFile, const std::string resultFile,
     const std::string logFile, const std::string runDir,
     const csi_inst::CoverageOptimizationGraph& graph,
     const std::set<llvm::BasicBlock*>* canProbe,
     const std::set<llvm::BasicBlock*>* wantData,
     const std::set<llvm::BasicBlock*>* crashPoints);

};

} // end csi_inst namespace

#endif
