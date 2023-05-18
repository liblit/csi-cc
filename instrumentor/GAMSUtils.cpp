//===--------------------------- GAMSUtils.cpp ----------------------------===//
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

#include "GAMSUtils.h"
#include "NaiveCoverageSet.h"
#include "Utils.hpp"

#include "gclgms.h"

#include "llvm_proxy/CFG.h"

#include <iostream>
#include <sstream>

using namespace csi_inst;
using namespace GAMS;
using namespace llvm;
using namespace std;


GAMSinterface::GAMSinterface(const string gamsdir) : sysdir(gamsdir){
  loadObjects();
}

void GAMSinterface::loadObjects(){
  // store GAMS initialization error messages
  string msg;
  
  if(!gamsx.Init(sysdir, msg))
    report_fatal_error("Could not create GAMS gamsx object: " + msg);
  if(!gdx.Init(sysdir, msg))
    report_fatal_error("Could not create GAMS gdx object: " + msg);
  if(!opt.Init(sysdir, msg))
    report_fatal_error("Could not create GAMS opt object: " + msg);
}

void GAMSinterface::writeSet(string name, string desc, set<string> data){
  string sp[GMS_MAX_INDEX_DIM];
  gdxValues_t v;
  
  if(!gdx.DataWriteStrStart(name, desc, 1, GMS_DT_SET, 0))
    report_fatal_error("write of set '" + name + "' failed");
  
  for(set<string>::iterator i = data.begin(), e = data.end(); i != e; ++i){
    sp[0] = *i;
    v[GMS_VAL_LEVEL] = 0;
    gdx.DataWriteStr(sp, v);
  }
  
  if(!gdx.DataWriteDone()) 
    report_fatal_error("failed to complete write of set '" + name +
                       "' in gdxDataWriteDone");
}

void GAMSinterface::writeSetOfPair(string name, string desc,
                                   set<pair<string, string> > data){
  string sp[GMS_MAX_INDEX_DIM];
  gdxValues_t v;
  
  if(!gdx.DataWriteStrStart(name, desc, 2, GMS_DT_SET, 0))
    report_fatal_error("write of set of pairs '" + name + "' failed");
  
  for(set<pair<string, string> >::iterator i = data.begin(), e = data.end(); i != e; ++i){
    sp[0] = i->first;
    sp[1] = i->second;
    v[GMS_VAL_LEVEL] = 0;
    gdx.DataWriteStr(sp, v);
  }
  
  if(!gdx.DataWriteDone()) 
    report_fatal_error("failed to complete write of set of pairs '" + name +
                       "' in gdxDataWriteDone");
}

void GAMSinterface::write2dSet(string name, string desc,
                               map<string, set<string> > data){
  string sp[GMS_MAX_INDEX_DIM];
  gdxValues_t v;
  
  if(!gdx.DataWriteStrStart(name, desc, 2, GMS_DT_SET, 0))
    report_fatal_error("write of 2d set '" + name + "' failed");
  
  for(map<string, set<string> >::iterator i = data.begin(), e = data.end();
      i != e; ++i){
    sp[0] = i->first;
    if(i->second.size() < 1)
      report_fatal_error("bad parameter passed to 2d set!");
    for(set<string>::iterator j = i->second.begin(), je = i->second.end();
        j != je; ++j){
      sp[1] = *j;
      v[GMS_VAL_LEVEL] = 0;
      gdx.DataWriteStr(sp, v);
    }
  }
  
  if(!gdx.DataWriteDone())
    report_fatal_error("failed to complete write of 2d set '" + name +
                       "' in gdxDataWriteDone");
}

void GAMSinterface::write4dSet(string name, string desc,
   map<string, map<string, map<string, set<string> > > >& data){
  string sp[GMS_MAX_INDEX_DIM];
  gdxValues_t v;
  
  if(!gdx.DataWriteStrStart(name, desc, 4, GMS_DT_SET, 0))
    report_fatal_error("write of 4d set '" + name + "' failed");
  
  for(map<string, map<string, map<string, set<string> > > >::iterator alpha =
         data.begin(), alpha_e = data.end(); alpha != alpha_e; ++alpha){
    sp[0] = alpha->first;
    map<string, map<string, set<string> > >& betas = alpha->second;
    for(map<string, map<string, set<string> > >::iterator beta =
         betas.begin(), beta_e = betas.end(); beta != beta_e; ++beta){
      sp[1] = beta->first;
      map<string, set<string> >& ds = beta->second;
      for(map<string, set<string> >::iterator d = ds.begin(), d_e = ds.end();
           d != d_e; ++d){
        sp[2] = d->first;
        set<string>& is = d->second;
        for(set<string>::iterator i = is.begin(), i_e = is.end(); i != i_e; ++i){
          sp[3] = *i;
          v[GMS_VAL_LEVEL] = 0;
          gdx.DataWriteStr(sp, v);
        }
      }
    }
  }
  
  if(!gdx.DataWriteDone())
    report_fatal_error("failed to complete write of 4d set '" + name +
                       "' in gdxDataWriteDone");
}

void GAMSinterface::writeParameter(string name, string desc,
                                   map<string, double> data){
  string sp[GMS_MAX_INDEX_DIM];
  gdxValues_t v;
  
  if(!gdx.DataWriteStrStart(name, desc, 1, GMS_DT_PAR, 0))
    report_fatal_error("write of param '" + name + "' failed");
  
  for(map<string, double>::iterator i = data.begin(), e = data.end();
      i != e; ++i){
    sp[0] = i->first;
    v[GMS_VAL_LEVEL] = i->second;
    gdx.DataWriteStr(sp, v);
  }
  
  if(!gdx.DataWriteDone()) 
    report_fatal_error("failed to complete write of param '" + name +
                       "' in gdxDataWriteDone");
}

void GAMSinterface::startParameterRead(string varName, int dimension){
  // error status/message variables
  string msg;
  
  int varNr;
  if(!gdx.FindSymbol(varName, varNr))
    report_fatal_error("could not find variable " + varName);
  
  int dim, varType;
  gdx.SymbolInfo(varNr, varName, dim, varType);
  if(dim != dimension || varType != GMS_DT_PAR){
    stringstream dimStream;
    dimStream << dimension;
    report_fatal_error(varName + " is not a " + dimStream.str() +
                       "-dimensional parameter");
  }
  
  int NrRecs;
  if(!gdx.DataReadStrStart(varNr, NrRecs)){
    gdx.ErrorStr(gdx.GetLastError(), msg);
    report_fatal_error("failed to start data read of variable '" +
                       varName + "': " + msg);
  }
}

void GAMSinterface::finishParameterRead(string varName){
  // error status/message variables
  int status;
  string msg;
  
  gdx.DataReadDone();
  status = gdx.GetLastError();
  if(status){
    gdx.ErrorStr(status, msg);
    report_fatal_error("error reading gdx file '" + varName + "': " + msg);
  }
}

set<BasicBlock*> GAMSinterface::readSolutionData(const string resultFile){
  // error status/message variables
  int status;
  string msg;
  
  gdx.OpenRead(resultFile, status);
  if(status){
    gdx.ErrorStr(status, msg);
    report_fatal_error("failed to open gdx file '" + resultFile + "' for " +
                       "reading: " + msg);
  }
  
  // parameter reading buffers
  int FDim;
  string sp[GMS_MAX_INDEX_DIM];
  double v[GMS_MAX_INDEX_DIM];
  
  startParameterRead("solveStat", 0);
  if(!gdx.DataReadStr(sp, v, FDim))
    report_fatal_error("no solve status reported by GAMS");
  if(v[GMS_VAL_LEVEL] != 1.0)
    report_fatal_error("GAMS solver solve status was not 1.0 (normal "
                       "completion)");
  if(gdx.DataReadStr(sp, v, FDim))
    report_fatal_error("multiple solve status reported by GAMS");
  finishParameterRead("solveStat");
  
  // TODO: update GAMS model so we get the "best possible" result (i.e. we cover
  // the largest possible number of blocks) if we can't get all of them
  startParameterRead("modelStat", 0);
  if(!gdx.DataReadStr(sp, v, FDim))
    report_fatal_error("no model status reported by GAMS");
  if(v[GMS_VAL_LEVEL] != 1.0)
    report_fatal_error("GAMS solver model status was not 1.0 (optimal)");
  if(gdx.DataReadStr(sp, v, FDim))
    report_fatal_error("multiple model status reported by GAMS");
  finishParameterRead("modelStat");
  
  
  startParameterRead("result", 1);
  set<BasicBlock*> result;
  while(gdx.DataReadStr(sp, v, FDim)){
    if(v[GMS_VAL_LEVEL] == 0.0)
      continue;
    
    map<string, BasicBlock*>::iterator found = blockNameMap.find(sp[0]);
    if(found == blockNameMap.end())
      report_fatal_error("Invalid basic block ('" + sp[0] + "') returned in "
                         "GAMS result");
    result.insert(found->second);
  }
  finishParameterRead("result");
  
  
  if(gdx.Close()){
    gdx.ErrorStr(gdx.GetLastError(), msg);
    report_fatal_error("failed to close gdx file '" + resultFile + "': " + msg);
  }
  
  return(result);
}

void GAMSinterface::callGams(const string gamsFile, const string resultFile,
                             const string logFile, const string runDir){
  string msg;
  string deffile = sysdir + "/optgams.def";
  
  if(opt.ReadDefinition(deffile)){
    stringstream errSStream;
    int i, itype;
    for(i = 1; i <= opt.MessageCount(); i++){
      opt.GetMessage(i, msg, itype);
      errSStream << msg << endl;
    }
    report_fatal_error(errSStream.str());
  }
  
  int saveEOLOnly, optNr, optRef;
  saveEOLOnly = opt.EOLOnlySet(0);  
  opt.ReadFromStr("I=" + gamsFile);
  opt.ReadFromStr("lo=0");
  opt.ReadFromStr("GDX=" + resultFile);
  opt.ReadFromStr("O=" + logFile);
  opt.ReadFromStr("WDir=" + runDir);
  opt.EOLOnlySet(saveEOLOnly);
  if(opt.FindStr("sysdir",optNr,optRef))
    opt.SetValuesNr(optNr,0,0.0,sysdir);
  
  if(gamsx.RunExecDLL(opt.GetHandle(), sysdir, 1, msg))
    report_fatal_error("Could not execute GAMS RunExecDLL: " +  msg);
}

// create the full Y_{\alpha \beta d} sets (provided as input to the GAMS model)
// TODO: pare this down, in parallel with removing redundant (alpha, beta, d)
void fillGiantYMap(const set<BasicBlock*>& canProbe,
                   const set<BasicBlock*>& wantData,
                   const set<BasicBlock*>& crashPoints,
                   const CoverageOptimizationGraph& graph,
                   map<BasicBlock*,
                         map<BasicBlock*,
                               map<BasicBlock*,
                                     set<BasicBlock*> > > >& result){
  BasicBlock* entryBlock = graph.getEntryBlock();

  set<BasicBlock*> alphas = canProbe;
  alphas.insert(entryBlock);
  set<BasicBlock*> betas = canProbe;
  betas.insert(crashPoints.begin(), crashPoints.end());

  for(set<BasicBlock*>::iterator alpha = alphas.begin(), alpha_e = alphas.end();
      alpha != alpha_e; ++alpha){
    for(set<BasicBlock*>::iterator beta = betas.begin(), beta_e = betas.end();
        beta != beta_e; ++beta){
      for(set<BasicBlock*>::iterator d = wantData.begin(), d_e = wantData.end();
          d != d_e; ++d){
        set<BasicBlock*>& thisOne = result[*alpha][*beta][*d];
        BasicBlock* thisAlpha = *alpha;
        BasicBlock* thisBeta = *beta;
        BasicBlock* thisD = *d;

        set<BasicBlock*> Y1 = connectedExcluding(
           set<BasicBlock*>(&entryBlock, &entryBlock+1),
           set<BasicBlock*>(&thisAlpha, &thisAlpha+1),
           set<BasicBlock*>(&thisD, &thisD+1));
        thisOne.insert(Y1.begin(), Y1.end());

        set<BasicBlock*> X_minus_d = crashPoints;
        X_minus_d.erase(thisD);
        set<BasicBlock*> Y2 = connectedExcluding(
           set<BasicBlock*>(&thisBeta, &thisBeta+1),
           X_minus_d,
           set<BasicBlock*>(&thisD, &thisD+1));
        thisOne.insert(Y2.begin(), Y2.end());
      }
    }
  }
}

void GAMSinterface::writeModelData(const string gdxFile,
                                   const CoverageOptimizationGraph& graph,
                                   const set<BasicBlock*>* canProbe,
                                   const set<BasicBlock*>* wantData,
                                   const set<BasicBlock*>* crashPoints){
  blockNameMap.clear();
  nameBlockMap.clear();
  
  Function* F = graph.getFunction();
  if(!F)
    report_fatal_error("GAMS error: graph data has no associated function");
  
  // *** nodes & cost ***
  // (also creates mappings block->name and name->block)
  set<string> gamsNodes;
  map<string, double> gamsCost;
  stringstream addrStream; // stream for extracting "names" from basic blocks
  unsigned long unnamedBlocks = 0; // used to make unique names for unnamed BBs
  for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
    // clear the stream from the previous address
    addrStream.str(string());
    
    // get the block address as a node as a string
    BasicBlock* node = &*i;
    //addrStream << (const void*)(node);
    string fullName = node->getName().str();
    fullName.erase(std::remove(fullName.begin(), fullName.end(), '.'), fullName.end());
    if(fullName.empty())
      addrStream << "CSIunnamedblock" << unnamedBlocks++;
    else
      addrStream << fullName;
    string nodeName = addrStream.str();
    if(blockNameMap.count(nodeName) || nameBlockMap.count(node))
      report_fatal_error("GAMS error: generated the same name ('" + nodeName +
                         "') for multiple basic blocks");
    blockNameMap[nodeName] = node;
    nameBlockMap[node] = nodeName;
    
    // insert the node and its cost
    gamsNodes.insert(nodeName);
    gamsCost[nodeName] = graph.getBlockCost(node);
    // TODO: make less of a hack.  This is due to a bug where some blocks
    // get assigned 0.0 cost; this happens in the case of a basic block
    // ending with "exit(n)".  See, e.g., print_tokens v1 (function
    // "keyword"), or test named "switch_return_exit.c"
    if(gamsCost[nodeName] == 0.0)
      gamsCost[nodeName] = 0.00001;
  }
  
  // *** entry node ***
  set<string> gamsEntry;
  gamsEntry.insert(nameBlockMap.at(graph.getEntryBlock()));
  
  // *** edges ***
  set<pair<string, string> > gamsEdges;
  for(Function::iterator i = F->begin(), e = F->end(); i != e; ++i){
    BasicBlock* node = &*i;
    if(!nameBlockMap.count(node))
      report_fatal_error("GAMS error: edge for missing node");

    const vector<BasicBlock*>& succs = graph.getBlockSuccs(node);
    for(vector<BasicBlock*>::const_iterator k = succs.begin(), ke = succs.end(); k != ke; ++k){
      if(nameBlockMap.count(*k) != 1)
        report_fatal_error("GAMS error: edge targetting missing node");

      gamsEdges.insert(make_pair(nameBlockMap[node], nameBlockMap[*k]));
    }
  }

  // *** desired ***
  set<string> gamsDesired;
  if(wantData == NULL){
    gamsDesired = gamsNodes;
  }
  else{
    for(set<BasicBlock*>::const_iterator i = wantData->begin(), e = wantData->end(); i != e; ++i){
      const map<BasicBlock*, string>::const_iterator found =
         nameBlockMap.find(*i);
      if(found == nameBlockMap.end())
        report_fatal_error("GAMS detected 'desired' node that does not exist "
                           "in the optimization graph");
      
      gamsDesired.insert(found->second);
    }
  }
  
  // *** can_inst ***
  set<string> gamsCanInst;
  if(canProbe == NULL){
    gamsCanInst = gamsNodes;
  }
  else{
    for(set<BasicBlock*>::const_iterator i = canProbe->begin(), e = canProbe->end(); i != e; ++i){
      const map<BasicBlock*, string>::const_iterator found =
         nameBlockMap.find(*i);
      if(found == nameBlockMap.end())
        report_fatal_error("GAMS detected 'canInst' node that does not exist "
                           "in the optimization graph");
      
      gamsCanInst.insert(found->second);
    }
  }

  // *** "a" set for Y_abdi ***
  map<string, map<string, map<string, set<string> > > > gamsA;
  map<BasicBlock*, map<BasicBlock*,
                   map<BasicBlock*, set<BasicBlock*> > > > bbGamsA;
  fillGiantYMap(*canProbe, *wantData, *crashPoints, graph, bbGamsA);
  map<BasicBlock*, string>::const_iterator found;
  for(map<BasicBlock*,
          map<BasicBlock*,
              map<BasicBlock*,
                  set<BasicBlock*> > > >::iterator alpha = bbGamsA.begin(),
                                                   alpha_e = bbGamsA.end();
      alpha != alpha_e; ++alpha){
    found = nameBlockMap.find(alpha->first);
    if(found == nameBlockMap.end())
      report_fatal_error("GAMS detected mising alpha");
    string alpha_str = found->second;
    map<BasicBlock*, map<BasicBlock*, set<BasicBlock*> > >& betas =
       alpha->second;
    for(map<BasicBlock*, map<BasicBlock*, set<BasicBlock*> > >::iterator beta =
           betas.begin(), beta_e = betas.end(); beta != beta_e; ++beta){
      found = nameBlockMap.find(beta->first);
      if(found == nameBlockMap.end())
        report_fatal_error("GAMS detected mising beta");
      string beta_str = found->second;
      map<BasicBlock*, set<BasicBlock*> >& ds = beta->second;
      for(map<BasicBlock*, set<BasicBlock*> >::iterator d = ds.begin(),
                                                        d_e = ds.end();
          d != d_e; ++d){
        found = nameBlockMap.find(d->first);
        if(found == nameBlockMap.end())
          report_fatal_error("GAMS detected mising d");
        string d_str = found->second;
        set<BasicBlock*>& is = d->second;
        for(set<BasicBlock*>::iterator i = is.begin(), i_e = is.end();
            i != i_e; ++i){
          found = nameBlockMap.find(*i);
          if(found == nameBlockMap.end())
            report_fatal_error("GAMS detected mising i (entry for Y)");
          string i_str = found->second;
          gamsA[alpha_str][beta_str][d_str].insert(i_str);
        }
      }
    }
  }

  // *** exit/crash nodes ***
  set<string> gamsExit;
  if(crashPoints == NULL){
    gamsExit = gamsNodes;
  }
  else{
    for(set<BasicBlock*>::const_iterator i = crashPoints->begin(), e = crashPoints->end(); i != e; ++i){
      const map<BasicBlock*, string>::const_iterator found =
         nameBlockMap.find(*i);
      if(found == nameBlockMap.end())
        report_fatal_error("GAMS detected exit/crash node that does not exist "
                           "in the optimization graph");
      
      gamsExit.insert(found->second);
    }
  }

  if(!gamsExit.size())
    report_fatal_error("GAMS error: no exit block for function " +
                       F->getName().str());

  // error status/message variables
  int status;
  string msg;
  
  // open gdx file for writing
  gdx.OpenWrite(gdxFile, "opening gdx file", status);
  if(status){
    gdx.ErrorStr(status, msg);
    report_fatal_error("failed to open gdx file '" + gdxFile + "' for " +
                       "writing: " + msg);
  }
  
  // write out our computed data into the gdx file
  writeSet("nodes", "Graph BBs/nodes", gamsNodes);
  writeSet("entry", "Graph entry BB", gamsEntry);
  writeSet("exit", "Graph exit BB", gamsExit);
  writeSetOfPair("edges", "Graph edges", gamsEdges);
  writeSet("desired", "Desired nodes", gamsDesired);
  writeSet("can_inst", "Nodes we are allowed to instrument", gamsCanInst);
  writeParameter("cost", "Node costs", gamsCost);
  write4dSet("a", "Flatted Y set of reachable nodes", gamsA);
  
  // close written gdx file
  if(gdx.Close()){
    gdx.ErrorStr(gdx.GetLastError(), msg);
    report_fatal_error("failed to close gdx file '" + gdxFile + "': " + msg);
  }
}

set<BasicBlock*> GAMSinterface::optimizeModel (const string gamsFile,
                                   const string gdxFile,
                                   const string resultFile,
                                   const string logFile,
                                   const string runDir,
                                   const CoverageOptimizationGraph& graph,
                                   const set<BasicBlock*>* canProbe,
                                   const set<BasicBlock*>* wantData,
                                   const set<BasicBlock*>* crashPoints){
  writeModelData(gdxFile, graph, canProbe, wantData, crashPoints);
  
  callGams(gamsFile, resultFile, logFile, runDir);
  
  return(readSolutionData(runDir + '/' + resultFile));
}
