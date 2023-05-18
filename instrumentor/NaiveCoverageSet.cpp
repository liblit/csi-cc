//===----------------------- NaiveCoverageSet.cpp -------------------------===//
//
// A very naive implementation of checking coverage sets.
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

#include "NaiveCoverageSet.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>

#include "llvm_proxy/CFG.h"

#include <queue>
#include <stack>

using namespace csi_inst;
using namespace llvm;
using namespace std;


bool csi_inst::isCoverageSet(const set<BasicBlock*>& S,
                             const set<BasicBlock*>& D,
                             BasicBlock* e,
                             const set<BasicBlock*>& X){
  set<BasicBlock*> alphas = S;
  alphas.insert(e);
  set<BasicBlock*> betas = S;
  betas.insert(X.begin(), X.end());

  // current version: iterating over "d" first reduces alphas and betas to use
  for(set<BasicBlock*>::const_iterator d = D.begin(), de = D.end(); d != de; ++d){
    BasicBlock* thisD = *d;
    if(S.count(thisD))
      continue;

    set<BasicBlock*> beforeD = connectedExcluding(set<BasicBlock*>(&e, &e+1),
                                             set<BasicBlock*>(&thisD, &thisD+1),
                                             set<BasicBlock*>());
    set<BasicBlock*> thisAlphas;
    set_intersection(beforeD.begin(), beforeD.end(),
                     alphas.begin(), alphas.end(),
                     std::inserter(thisAlphas, thisAlphas.begin()));
  
    set<BasicBlock*> afterD = connectedExcluding(set<BasicBlock*>(&thisD,
                                                                  &thisD+1),
                                                 betas,
                                                 set<BasicBlock*>());
    set<BasicBlock*> thisBetas;
    set_intersection(afterD.begin(), afterD.end(),
                     betas.begin(), betas.end(),
                     std::inserter(thisBetas, thisBetas.begin()));

    for(set<BasicBlock*>::const_iterator alpha = thisAlphas.begin(), ae = thisAlphas.end(); alpha != ae; ++alpha){
      if(*d == *alpha)
        continue;
      for(set<BasicBlock*>::const_iterator beta = thisBetas.begin(), be = thisBetas.end(); beta != be; ++beta){
        if(*d == *beta)
          continue;
        else if(hasAmbiguousTriangle(*alpha, *beta, *d, e, X, S)){
          return(false);
        }
      }
    }
  }

  return(true);
}

bool csi_inst::isCoverageSetClose(const set<BasicBlock*>& S,
                                  const set<BasicBlock*>& D,
                                  BasicBlock* e,
                                  const set<BasicBlock*>& X){
  set<BasicBlock*> alphas = S;
  alphas.insert(e);
  set<BasicBlock*> betas = S;
  betas.insert(X.begin(), X.end());

  for(set<BasicBlock*>::const_iterator d = D.begin(), de = D.end(); d != de; ++d){
    BasicBlock* thisD = *d;
    if(S.count(thisD))
      continue;

    set<BasicBlock*> firstAlphas = firstTwoEncountered(thisD, alphas, false);

    set<BasicBlock*> firstBetas = firstTwoEncountered(thisD, betas, true);

    for(set<BasicBlock*>::const_iterator alpha = firstAlphas.begin(), ae = firstAlphas.end(); alpha != ae; ++alpha){
      if(*d == *alpha)
        continue;
      for(set<BasicBlock*>::const_iterator beta = firstBetas.begin(), be = firstBetas.end(); beta != be; ++beta){
        if(*d == *beta)
          continue;
        else if(hasAmbiguousTriangle(*alpha, *beta, *d, e, X, S)){
          return(false);
        }
      }
    }
  }

  return(true);
}

set<BasicBlock*> csi_inst::firstEncountered(
     BasicBlock* from,
     const set<BasicBlock*>& to,
     bool forward){
  set<BasicBlock*> result;
  oneHop(from, to, forward, result);
  return(result);
}

set<BasicBlock*> csi_inst::firstTwoEncountered(
     BasicBlock* from,
     const set<BasicBlock*>& to,
     bool forward){
  set<BasicBlock*> result;
  oneHop(from, to, forward, result);
  set<BasicBlock*> secondResult = result;
  for(set<BasicBlock*>::iterator i = result.begin(), e = result.end(); i != e; ++i)
    oneHop(*i, to, forward, secondResult);
  return(secondResult);
}

void csi_inst::oneHop(
     BasicBlock* from,
     const set<BasicBlock*>& to,
     bool forward,
     set<BasicBlock*>& result){
  set<BasicBlock*> visited = to;

  // search forward/backward from "from"
  queue<BasicBlock*> worklist;
  worklist.push(from);
  if(forward){
    for(succ_iterator i = succ_begin(from), e = succ_end(from); i != e; ++i)
      worklist.push(*i);
  }
  else{
    for(pred_iterator i = pred_begin(from), e = pred_end(from); i != e; ++i)
      worklist.push(*i);
  }
  while(!worklist.empty()){
    BasicBlock* n = worklist.front();
    worklist.pop();
    if(to.count(n))
      result.insert(n);
    if(visited.count(n))
      continue;
    else
      visited.insert(n);

    if(forward){
      for(succ_iterator i = succ_begin(n), e = succ_end(n); i != e; ++i)
        worklist.push(*i);
    }
    else{
      for(pred_iterator i = pred_begin(n), e = pred_end(n); i != e; ++i)
        worklist.push(*i);
    }
  }
}

bool csi_inst::hasAmbiguousTriangle(BasicBlock* alpha,
                                    BasicBlock* beta,
                                    BasicBlock* d,
                                    BasicBlock* e,
                                    const set<BasicBlock*>& X,
                                    const set<BasicBlock*>& S){
  set<BasicBlock*> X_minus_d = X;
  X_minus_d.erase(d);

  set<BasicBlock*> Y1 = connectedExcluding(set<BasicBlock*>(&e, &e+1),
                                           set<BasicBlock*>(&alpha, &alpha+1),
                                           set<BasicBlock*>(&d, &d+1));
  set<BasicBlock*> Y2 = connectedExcluding(set<BasicBlock*>(&beta, &beta+1),
                                           X_minus_d,
                                           set<BasicBlock*>(&d, &d+1));
  if(Y1.empty() || Y2.empty())
    return(false);
  
  // here, we would compute the Y set, but all we actually care about is S\Y
  set<BasicBlock*> S_minus_Y = S;
  for(set<BasicBlock*>::iterator i = Y1.begin(), ie = Y1.end(); i != ie; ++i)
    S_minus_Y.erase(*i);
  for(set<BasicBlock*>::iterator i = Y2.begin(), ie = Y2.end(); i != ie; ++i)
    S_minus_Y.erase(*i);

  if(!isConnectedExcluding(set<BasicBlock*>(&alpha, &alpha+1),
                           set<BasicBlock*>(&d, &d+1), S_minus_Y))
    return(false);
  else if(!isConnectedExcluding(set<BasicBlock*>(&d, &d+1),
                                set<BasicBlock*>(&beta, &beta+1), S_minus_Y))
    return(false);

  S_minus_Y.insert(d);
  if(!isConnectedExcluding(set<BasicBlock*>(&alpha, &alpha+1),
                           set<BasicBlock*>(&beta, &beta+1), S_minus_Y))
    return(false);

  DEBUG(dbgs() << "Found triangle: (" << alpha->getName().str() << ", "
               << beta->getName().str() << ","
               << d->getName().str() << ")\n");
  DEBUG(dbgs() << "With S = " << setBB_asstring(S)
               << "\nand S\\Y = " << setBB_asstring(S_minus_Y) << "\n");
  return(true);
}

bool csi_inst::isConnectedExcluding(const set<BasicBlock*>& from,
                                    const set<BasicBlock*>& to,
                                    const set<BasicBlock*>& excluding){
  // check for disjointness of "from" and "to"
  for(set<BasicBlock*>::const_iterator i = from.begin(), e = from.end(); i != e; ++i){
    if(to.count(*i))
      return(true);
  }

  set<BasicBlock*> visited = from;
  queue<BasicBlock*> worklist;
  for(set<BasicBlock*>::const_iterator i = from.begin(), e = from.end(); i != e; ++i){
    for(succ_iterator j = succ_begin(*i), je = succ_end(*i); j != je; ++j)
      worklist.push(*j);
  }
  while(!worklist.empty()){
    BasicBlock* n = worklist.front();
    worklist.pop();
    if(visited.count(n) || excluding.count(n))
      continue;
    else if(to.count(n))
      return(true);
    else
      visited.insert(n);

    for(succ_iterator i = succ_begin(n), e = succ_end(n); i != e; ++i)
      worklist.push(*i);
  }

  return(false);
}

set<BasicBlock*> csi_inst::connectedExcluding(
     const set<BasicBlock*>& from,
     const set<BasicBlock*>& to,
     const set<BasicBlock*>& excluding){
  set<BasicBlock*> visitedFW = from;
  set<BasicBlock*> visitedBW = to;

  // search forward from "from"
  queue<BasicBlock*> worklist;
  for(set<BasicBlock*>::const_iterator i = from.begin(), e = from.end(); i != e; ++i){
    for(succ_iterator j = succ_begin(*i), je = succ_end(*i); j != je; ++j)
      worklist.push(*j);
  }
  while(!worklist.empty()){
    BasicBlock* n = worklist.front();
    worklist.pop();
    if(visitedFW.count(n) || excluding.count(n))
      continue;
    else
      visitedFW.insert(n);

    for(succ_iterator i = succ_begin(n), e = succ_end(n); i != e; ++i)
      worklist.push(*i);
  }

  // search backward from "to"
  for(set<BasicBlock*>::const_iterator i = to.begin(), e = to.end(); i != e; ++i){
    for(pred_iterator j = pred_begin(*i), je = pred_end(*i); j != je; ++j)
      worklist.push(*j);
  }
  while(!worklist.empty()){
    BasicBlock* n = worklist.front();
    worklist.pop();
    if(visitedBW.count(n) || excluding.count(n))
      continue;
    else
      visitedBW.insert(n);

    for(pred_iterator i = pred_begin(n), e = pred_end(n); i != e; ++i)
      worklist.push(*i);
  }

  set<BasicBlock*> result;
  set_intersection(visitedFW.begin(), visitedFW.end(),
                   visitedBW.begin(), visitedBW.end(),
                  std::inserter(result, result.begin()));
  return(result);
}
