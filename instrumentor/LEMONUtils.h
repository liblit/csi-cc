//===--------------------------- LEMONUtils.h -----------------------------===//
//
// Utilities for interfacing with the LEMON graph/optimization framework.
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
#ifndef CSI_LEMON_UTILS_H
#define CSI_LEMON_UTILS_H

#include "CoverageOptimizationGraph.h"


#include <lemon/list_graph.h>
#ifdef USE_GUROBI
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <gurobi_c++.h>
#pragma GCC diagnostic pop
#endif

#include <set>

namespace csi_inst {

class LEMONtriangle;

class LEMONsolver{
private:
  typedef lemon::ListDigraph Graph;
  typedef Graph::Node GraphNode;
  typedef Graph::Arc GraphEdge;
  typedef Graph::NodeMap<double> BlockCostMap;
  typedef Graph::ArcMap<double> EdgeCostMap;

  // the LEMON version of the LLVM control-flow graph
  Graph graph;
  // its entry node
  GraphNode graphEntry;
  // its node->cost map
  BlockCostMap nodeCostMap;

  // a mapping from LEMON nodes to LLVM basic blocks (filled by
  // the constructor)
  std::map<GraphNode, llvm::BasicBlock*> lemonToLlvmMap;
  // a mapping from LLVM basic blocks to LEMON nodes (filled by
  // the constructor)
  std::map<llvm::BasicBlock*, GraphNode> llvmToLemonMap;

  // convert a set of LLVM Basic Blocks into LEMON graph nodes (based on the
  // mapping data from the constructor...in llvmToLemonMap)
  // If blockSet is NULL, then return the set of all nodes in the graph
  std::set<GraphNode> llvmSetToLemonNodeSet(
   const std::set<llvm::BasicBlock*>* blockSet,
   const std::string setName);

  // dump the LEMON graph to the provided file
  void dumpGraph(const std::string dumpFile);

  // Map the Lemon nodes to gurobi variables
  std::map<GraphNode, GRBVar> lemonToGRBMap;
  // And (since GRBVar does not contain operator<, we use integer map back
  std::map<int, GraphNode> GRBIndexToLemonMap;
  
  // helper method to add constraints to MIP
  int addConsToMIP(const std::set<LEMONtriangle> &tri, const std::set<GraphNode> &I, GRBModel &model);

  // These were counters for the class.  Might be too hard to pass it up.
  // Changing logging
  //double ipTime;
  //double triangleTime;

public:
  // constructor
  LEMONsolver(const csi_inst::CoverageOptimizationGraph& graph);

  std::set<llvm::BasicBlock*> optimize(
     const std::set<llvm::BasicBlock*>* canProbe,
     const std::set<llvm::BasicBlock*>* wantData,
     const std::set<llvm::BasicBlock*>* crashPoints,
     bool logStats=false);

  // this is public for testing: eventually, it should be private
  std::set<GraphNode> optimize(const std::set<GraphNode>& canProbe,
                               const std::set<GraphNode>& wantData,
                               const std::set<GraphNode>& crashPoints,
			       bool logStats=false);

  // this is for testing: eventually, it should be removed
  LEMONsolver(const Graph& graph);

};



} // end csi_inst namespace

#endif
