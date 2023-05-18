//===-------------------------- LEMONUtils.cpp ----------------------------===//
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
#define DEBUG_TYPE "mip-tracing"

#include "LEMONUtils.h"
#include "LEMONCoverageSet.h"
#include "Utils.hpp"

#include <lemon/lgf_writer.h>

#include "llvm_proxy/CFG.h"
#include "llvm_proxy/DebugInfo.h"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <iostream>
#include <sstream>

using namespace csi_inst;
using namespace lemon;
using namespace llvm;
using namespace std;


LEMONsolver::LEMONsolver(const CoverageOptimizationGraph& inGraph) :
   graph(),
   nodeCostMap(graph) {
  Function* F = inGraph.getFunction();
  if(!F)
    report_fatal_error("LEMON error: graph data has no associated function");

  // add nodes and their costs from LLVM graph to LEMON graph
  for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
    BasicBlock* llvmNode = &*i;
    GraphNode lemonNode = graph.addNode();

    if(lemonToLlvmMap.count(lemonNode) || llvmToLemonMap.count(llvmNode))
      report_fatal_error("LEMON error: encountered the same LEMON or LLVM "
                         "node multiple times: (" + llvmNode->getName().str() +
                         ", " + to_string(graph.id(lemonNode)) + ')');
    lemonToLlvmMap[lemonNode] = llvmNode;
    llvmToLemonMap[llvmNode] = lemonNode;

    nodeCostMap[lemonNode] = inGraph.getBlockCost(llvmNode);
    // a bit of a hack.  This is due to an LLVM bug (?) where some blocks
    // get assigned 0.0 cost; this happens in the case of a basic block
    // ending with "exit(n)".  See, e.g., print_tokens v1 (function
    // "keyword"), or test named "switch_return_exit.c"
    if(nodeCostMap[lemonNode] == 0.0)
      nodeCostMap[lemonNode] = 0.00001;
  }

  // record the entry node
  graphEntry = llvmToLemonMap.at(inGraph.getEntryBlock());

  // add the edges
  for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
    BasicBlock* src = &*i;
    if(!llvmToLemonMap.count(src))
      report_fatal_error("LEMON error: edge for missing node");

    const vector<BasicBlock*>& succs = inGraph.getBlockSuccs(src);
    for(vector<BasicBlock*>::const_iterator k = succs.begin(), ke = succs.end(); k != ke; ++k){
      if(llvmToLemonMap.count(*k) != 1)
        report_fatal_error("LEMON error: edge targetting missing node");

      graph.addArc(llvmToLemonMap[src], llvmToLemonMap[*k]);
    }
  }

}

LEMONsolver::LEMONsolver(const Graph& inGraph) : graph(), nodeCostMap(graph) {
  digraphCopy(inGraph, graph).run();
}

void LEMONsolver::dumpGraph(string dumpFile){
  digraphWriter(graph, dumpFile).run();
}

set<LEMONsolver::GraphNode> LEMONsolver::llvmSetToLemonNodeSet(
   const set<BasicBlock*>* blockSet, const string setName){
  set<GraphNode> lemonSet;
  if(blockSet == NULL){
    // sadly, LEMON iterators don't use dereference, so they don't work with STL
    for(ListDigraph::NodeIt i(graph); i != INVALID; ++i)
      lemonSet.insert(i);
  }
  else{
    for(set<BasicBlock*>::const_iterator i = blockSet->begin(),
                                         e = blockSet->end();
        i != e; ++i){
      const map<BasicBlock*, GraphNode>::const_iterator found =
         llvmToLemonMap.find(*i);
      if(found == llvmToLemonMap.end())
        report_fatal_error("LEMON detected '" + setName + "' node that does "
                           "not exist in the optimization graph");

      lemonSet.insert(found->second);
    }
  }

  return(lemonSet);
}

#undef GUROBI_LOGGING
#undef WRITE_LP_FILES


#include <sys/time.h>
#include <sys/resource.h>

set<LEMONsolver::GraphNode> LEMONsolver::optimize(const set<GraphNode>& canProbe, const set<GraphNode>& wantData, const set<GraphNode>& crashPoints, bool logStats) {

  // wantData is D
  // crashPoints are T (or X in the ASE 2016 paper)
  // canProbe is I

  struct rusage usage;
  struct timeval tval;
  double start, end;


  double ipTime = 0.0;
  double triangleTime = 0.0;
  double totalIpTime = 0.0;
  double totalTriangleTime = 0.0;
  
  set<LEMONsolver::GraphNode> optCoverage;
  string function_name;
  function_name = lemonToLlvmMap.at(graphEntry)->getParent()->getName().str();

  // Initialize.  Make lists of vertices for original constraints

  ListDigraph::NodeMap<double> S(graph);
  
  // For timing, use wall time (not rusage/CPU-TIME)
  if (logStats) {
    getrusage(RUSAGE_SELF, &usage);
    tval = usage.ru_utime;
    start = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6);
  }

  set<LEMONtriangle> initialTriangles;
  // last 3 params is max depth, start depth, and max triangle/desired node
  //  (This will give 1 per desired node / depth)
  set<LEMONtriangle> t = getTriangles(graph, S, wantData, crashPoints, graphEntry, 0, 0, 0);
  initialTriangles.insert(t.begin(), t.end());
  
  if (logStats) {
    getrusage(RUSAGE_SELF, &usage);
    tval = usage.ru_utime;
    end = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6); 
    DEBUG(dbgs() << "Initial Triangles in " << (end-start) << " sec\n");
    triangleTime = end-start;
    totalTriangleTime += triangleTime;
  }

  // Now build the Gurobi Model
  GRBEnv *env = 0;
  GRBVar *x = 0;
  auto_ptr<GRBModel> model;
  try{
    env = new GRBEnv(true);
    env->set(GRB_IntParam_OutputFlag, 0);
    env->start();

    model.reset(new GRBModel(*env));
    // minimize
    model->set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
    // turnoff logging to console
    model->set(GRB_IntParam_LogToConsole, 0);
#if defined(GUROBI_LOGGING)    
    model->set(GRB_IntParam_LogToConsole, 1);
#endif
    model->set(GRB_IntParam_Threads,1);
  }
  catch(const GRBException& e){
    report_fatal_error("GUROBI license invalid, or GUROBI is not installed "
                       "corectly.  Message:\n" + e.getMessage());
  }

  // Add the variables
  unsigned int numVars = canProbe.size();
  x = new GRBVar[numVars];
  unsigned int nv = 0;
  for(set<GraphNode>::iterator it = canProbe.begin(); it != canProbe.end(); ++it) {
    x[nv] = model->addVar(0.0, 1.0, nodeCostMap[*it], GRB_BINARY);
    lemonToGRBMap[*it] = x[nv];
    GRBIndexToLemonMap[nv] = *it;
    nv++;
  }
  assert(nv == numVars);

  // Add the (initial) constraints
  int retval = addConsToMIP(initialTriangles, canProbe, *model);
  if (retval != 0) {
    // TODO: better error handling if the problem is infeasible
    // (This should never happen for an LLVM-produced control-flow graph.)
    assert(0);
  }

  // Solve it!  (i.e., Run Gurobi!)
  const int MAX_ITERATIONS = 200;
  const int INITIAL_MAX_DEPTH = 1;
  const int MAX_DEPTH_INCREMENT = 1;
  const int FINAL_MAX_DEPTH = 7;

  if(logStats){
    dbgs() << "EKK1000: " << function_name << ','

    // graph size information
         << countNodes(graph) <<  ',' << countArcs(graph)

    // internal parameter settings
         << ',' << MAX_ITERATIONS << ',' << INITIAL_MAX_DEPTH << ','
         << MAX_DEPTH_INCREMENT << ',' << FINAL_MAX_DEPTH

    // initial triangle generation information
         << ',' << initialTriangles.size() << ',' << triangleTime << "\n";
  }
  
  
  bool optimal = false;
  int iteration = 0;
  while (!optimal && iteration < MAX_ITERATIONS) {

    if (logStats) {
      getrusage(RUSAGE_SELF, &usage);
      tval = usage.ru_utime;
      start = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6);
    }
    
    model->optimize();
    
    if (logStats) {
      getrusage(RUSAGE_SELF, &usage);
      tval = usage.ru_utime;
      end = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6);
    
      DEBUG(dbgs() << "IP Time: " << (end-start) << " sec\n");
      
      ipTime = end-start;
      totalIpTime += ipTime;
    }
    
#if defined(WRITE_LP_FILES)
    char lpfile[128];
    sprintf(lpfile, "LP-%d.lp", iteration);
    string lpf(lpfile);
    model->write(lpfile);
#endif

    int status = model->get(GRB_IntAttr_Status);
    if (status != GRB_OPTIMAL) {
      // TODO: better error handling for a failed solve, though, again,
      // this should never happen for our graphs and structure
      report_fatal_error("internal error: optimization return status not optimal #" +
                         to_string(status) + ". Report this.\n");
    }
    else {
      double zip =  model->get(GRB_DoubleAttr_ObjVal);
      double num_nodes = model->get(GRB_DoubleAttr_NodeCount);
      ListDigraph::NodeMap<double> W(graph);

      for(unsigned int j = 0; j < numVars; j++) {
	      double xjip = x[j].get(GRB_DoubleAttr_X);
	      if (xjip > 0.001) {
	        W[GRBIndexToLemonMap[j]] = xjip;
	      }
      }

      if (logStats) {
        if (iteration == 0) {
          dbgs() << "EKK1001: (MaxDepth:) " << getMaxDistance(graph, W, wantData, crashPoints, graphEntry) << "\n";
        }

        getrusage(RUSAGE_SELF, &usage);
        tval = usage.ru_utime;
        start = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6);
      }

      set<LEMONtriangle> t;

      int depth = INITIAL_MAX_DEPTH;

      // This loop gets triangles up to a specified depth
      for(; depth <= FINAL_MAX_DEPTH; depth += MAX_DEPTH_INCREMENT) {
        // get triangles at exactly the current depth
        // (we've already checked 1..depth-1)
        // currently, this returns up to 7 triangles at that depth
        t = getTriangles(graph, W, wantData, crashPoints, graphEntry, depth, depth, 0, 7);
        if (t.size() > 0) break;
      }

      // Need to do it in full
      if (t.size() == 0) {
        depth = 0;
        // here: 1 triangle per desired node per depth
        t = getTriangles(graph, W, wantData, crashPoints, graphEntry, 0, 0, 0, 1);
      }

      if (logStats) {
        getrusage(RUSAGE_SELF, &usage);
        tval = usage.ru_utime;
        end = (double) tval.tv_sec + ((double)tval.tv_usec * 1e-6);

        triangleTime = end-start;
        totalTriangleTime += triangleTime;
      }

      if (t.size() == 0) {
        optimal = true;
      }
      else {
        int retval = addConsToMIP(t, canProbe, *model);
        if (retval != 0) {
          assert(0);
        }
      }

      if(logStats){
        dbgs() << "EKK1003: " << function_name << ',' << iteration << ',' << zip << ',' << num_nodes << ',' << ipTime << ',' << t.size() << ',' << triangleTime << ',' << depth << "\n";
      }
    }
    iteration++;
  }

  if (optimal) {
    DEBUG(dbgs() << "Optimal solution of value: " << model->get(GRB_DoubleAttr_ObjVal) << "\n");
    DEBUG(dbgs() << "Nodes in optimal coverage set: " << "\n");

    int numProbe = 0;
    for(unsigned int j = 0; j < numVars; j++) {
      double xjip = x[j].get(GRB_DoubleAttr_X);
      if (xjip > 0.001) {
	      numProbe++;
	      optCoverage.insert(GRBIndexToLemonMap[j]);
	      DEBUG(dbgs() << ' ' << graph.id(GRBIndexToLemonMap[j]));
      }
    }
    DEBUG(dbgs() << "\n");

    if(logStats){
      dbgs() << "EKK1004: " << function_name << ',' << numProbe << ','
             << model->get(GRB_DoubleAttr_ObjVal) << ','
             << totalIpTime << ',' << totalTriangleTime << "\n";
    }

  }
  
  delete [] x;
  return(optCoverage);
}

set<BasicBlock*> LEMONsolver::optimize(const set<BasicBlock*>* canProbe,
                                       const set<BasicBlock*>* wantData,
                                       const set<BasicBlock*>* crashPoints,
				       bool logStats){
  set<GraphNode> I = llvmSetToLemonNodeSet(canProbe, "canInst");
  set<GraphNode> D = llvmSetToLemonNodeSet(wantData, "desired");
  set<GraphNode> X = llvmSetToLemonNodeSet(crashPoints, "exit/crash");
  
  set<GraphNode> lemonResult = optimize(I, D, X, logStats);
  set<BasicBlock*> llvmResult;
  for(set<GraphNode>::iterator i = lemonResult.begin(), e = lemonResult.end();
      i != e; ++i){
    const map<GraphNode, BasicBlock*>::const_iterator found =
       lemonToLlvmMap.find(*i);
    if(found == lemonToLlvmMap.end())
      report_fatal_error("Invalid basic block returned in LEMON result");

    llvmResult.insert(found->second);
  }
  return(llvmResult);
}

int 
LEMONsolver::addConsToMIP(const set<LEMONtriangle> &tri, const set<GraphNode> &I,
			  GRBModel &model)
{
  int retval = 0;
  // Add constraints for triangles
  for(set<LEMONtriangle>::iterator it = tri.begin(); it != tri.end(); ++it) {
    set<GraphNode> V = it->getSymmetricDifference();
    vector<GraphNode> nodes;
    set_intersection(V.begin(), V.end(), I.begin(), I.end(), back_inserter(nodes));
    if (nodes.size() == 0) {
      // This should mean that the problem is infeasible, but...
      // As above, this should not happen for our instances, so report a clear
      // error message
      report_fatal_error("internal error: Problem is infeasible, add some "
                         "more possible probing nodes. Report this.\n");
      retval = -1;
      break;
    }
    else {
      GRBLinExpr lhs = 0;
      for(unsigned int i = 0; i < nodes.size(); i++) {
	      lhs += lemonToGRBMap[nodes[i]];
      }
      model.addConstr(lhs, GRB_GREATER_EQUAL, 1.0);
    }
  }
  return(retval);
}
