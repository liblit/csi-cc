//===----------------------- LEMONCoverageSet.cpp -------------------------===//
//
// An implementation of finding triangles and checking coverage sets using
// LEMON graphs.
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

#include "LEMONCoverageSet.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>

#include <lemon/dijkstra.h>

#include <algorithm>
#include <climits>
#include <queue>

using namespace csi_inst;
using namespace lemon;
using namespace llvm;
using namespace std;

typedef ListDigraph::Node GraphNode;

static cl::opt<bool> NoLEMONHeuristics("opt-no-heuristics", cl::Hidden,
        cl::desc("Don't use heuristics to help out the LEMON solver"));


// If vectors v1 and v2 share any nodes in common, trim to the meet-up point.
// If trimFromFront is true, trim from the front of each vector (i.e., remove
// nodes from v[0] to the meet-up point); otherwise, trim from the back (i.e.,
// remove nodes from the meet-up point to v[v.length])
//
// NOTE: this function modifies its arguments in-place
void trimToCommon(vector<GraphNode>& v1, vector<GraphNode>& v2,
                  bool trimFromFront){
  // TODO: remove loop someday, with full confidence in results

  // We make no guarantee that we will trim the most possible nodes here.
  // Instead, we just find a match (meet-up point), trim, and repeat until no
  // more trims are posssible.

  // Note that we do not use STL find_first_of because we need to know the
  // meet-up location in *both* vectors for trimming: We would need to do
  // an extra pass through v2 to find the meet-up point.  This would make the
  // code easier to read, but would be a tiny bit wasteful.
  bool didTrim = true;
  unsigned int trimCount = 0;
  while(didTrim){
    didTrim = false;
    if(trimFromFront){
      vector<GraphNode>::const_iterator v1Loc = v1.end();
      while(v1Loc != v1.begin()){
        --v1Loc;

        // find meet-up point
        vector<GraphNode>::const_iterator v2Loc = v2.end();
        while(v2Loc != v2.begin()){
          --v2Loc;
          if(*v1Loc == *v2Loc){
            // if found, trim vectors
            v1.erase(v1.begin(), v1Loc+1);
            v2.erase(v2.begin(), v2Loc+1);
            didTrim = true;
            ++trimCount;
            break;
          }
        }
        
        if(didTrim)
          break;
      }
    }
    else{
      for(vector<GraphNode>::const_iterator v1Loc = v1.begin(), e = v1.end();
          v1Loc != e; ++v1Loc){
        // find meet-up point
        vector<GraphNode>::const_iterator v2Loc = find(v2.begin(),
                                                       v2.end(),
                                                       *v1Loc);
        // if found, trim vectors
        if(v2Loc != v2.end()){
          v1.erase(v1Loc, v1.end());
          v2.erase(v2Loc, v2.end());
          didTrim = true;
          ++trimCount;
          break;
        }
      }
    }
  }

  // only one trim should be possible.  Rather than only *do* one trim, for now,
  // we'll verify that it works as expected
  if(trimCount > 1){
    report_fatal_error("internal LEMON error: unexpected multiple-trimming "
                       "paths for LEMON optimization.  Report this.\n");
  }
}


// ------------------------------- //
// ----- LEMONtriangle class ----- //
// ------------------------------- //
set<GraphNode> pathToSetOfNode(const Path<ListDigraph>& thePath,
                               const ListDigraph& graph){
  set<GraphNode> result;
  for(Path<ListDigraph>::ArcIt a(thePath); a != INVALID; ++a)
    result.insert(graph.target(a));
  return(result);
}

vector<GraphNode> pathToVectorOfNode(const Path<ListDigraph>& thePath,
                                     const ListDigraph& graph){
  vector<GraphNode> result;
  for(Path<ListDigraph>::ArcIt a(thePath); a != INVALID; ++a)
    result.push_back(graph.target(a));
  return(result);
}

LEMONtriangle::LEMONtriangle(double weight,
                             const Path<ListDigraph>& alphaD,
                             const Path<ListDigraph>& dBeta,
                             const Path<ListDigraph>& alphaBeta,
                             const set<GraphNode>& Y,
                             const ListDigraph& graph) : totalWeight(weight),
                                                         symDiff() {
  set<GraphNode> alphaDNodes;
  set<GraphNode> dBetaNodes;
  set<GraphNode> alphaBetaNodes;

  if(NoLEMONHeuristics){
    // just use the paths exactly as we found them to get the set of nodes
    // along each
    alphaDNodes = pathToSetOfNode(alphaD, graph);
    dBetaNodes = pathToSetOfNode(dBeta, graph);
    alphaBetaNodes = pathToSetOfNode(alphaBeta, graph);
  }
  else {
    vector<GraphNode> alphaDVector = pathToVectorOfNode(alphaD, graph);
    vector<GraphNode> dBetaVector = pathToVectorOfNode(dBeta, graph);
    vector<GraphNode> alphaBetaVector = pathToVectorOfNode(alphaBeta, graph);

    // "squeeze" alpha and beta closer to d by removing spurious differences that
    // cause weak constraints to be generated.
    // Specifically, if the legs of the triangle meet up anywhere besides alpha
    // and beta, discard the paths before/after the re-join point
    trimToCommon(alphaDVector, alphaBetaVector, true);
    trimToCommon(dBetaVector, alphaBetaVector, false);

    // then gather the set of nodes still left along each path into a set
    alphaDNodes = set<GraphNode>(alphaDVector.begin(), alphaDVector.end());
    dBetaNodes = set<GraphNode>(dBetaVector.begin(), dBetaVector.end());
    alphaBetaNodes = set<GraphNode>(alphaBetaVector.begin(), alphaBetaVector.end());

    if(alphaDNodes.empty()){
      // this should never happen: at a minimum, this set should contain the
      // node "d" itself, since the paths contain the *targets* of all edges
      report_fatal_error("internal LEMON error: heuristic LEMON trimming "
                         "resulted in a loss of D as instrumentable. "
                         "Report this.\n");
    }
  }

  // compute the symmetric difference
  set<GraphNode> alphaDBetaNodes;
  set_union(alphaDNodes.begin(), alphaDNodes.end(),
            dBetaNodes.begin(), dBetaNodes.end(),
            inserter(alphaDBetaNodes,alphaDBetaNodes.end()));
  set<GraphNode> fullSymDiff;
  set_symmetric_difference(alphaDBetaNodes.begin(), alphaDBetaNodes.end(),
                           alphaBetaNodes.begin(), alphaBetaNodes.end(),
                           inserter(fullSymDiff, fullSymDiff.end()));
  set_difference(fullSymDiff.begin(), fullSymDiff.end(),
                 Y.begin(), Y.end(),
                 inserter(symDiff, symDiff.end()));
}


// ----------------------------------- //
// ----- Other Exposed Functions ----- //
// ----------------------------------- //
set<GraphNode> csi_inst::connectedExcluding(const ListDigraph& graph,
                                            const set<GraphNode>& from,
                                            const set<GraphNode>& to,
                                            const set<GraphNode>& excluding){
  set<GraphNode> visitedFW = from;
  set<GraphNode> visitedBW = to;

  // search forward from "from"
  queue<GraphNode> worklist;
  for(set<GraphNode>::const_iterator i = from.begin(), e = from.end(); i != e; ++i){
    for(ListDigraph::OutArcIt a(graph, *i); a != INVALID; ++a)
      worklist.push(graph.target(a));
  }
  while(!worklist.empty()){
    GraphNode n = worklist.front();
    worklist.pop();
    if(visitedFW.count(n) || excluding.count(n))
      continue;
    else
      visitedFW.insert(n);

    for(ListDigraph::OutArcIt a(graph, n); a != INVALID; ++a)
      worklist.push(graph.target(a));
  }

  // search backward from "to"
  for(set<GraphNode>::const_iterator i = to.begin(), e = to.end(); i != e; ++i){
    for(ListDigraph::InArcIt a(graph, *i); a != INVALID; ++a)
      worklist.push(graph.source(a));
  }
  while(!worklist.empty()){
    GraphNode n = worklist.front();
    worklist.pop();
    if(visitedBW.count(n) || excluding.count(n))
      continue;
    else
      visitedBW.insert(n);

    for(ListDigraph::InArcIt a(graph, n); a != INVALID; ++a)
      worklist.push(graph.source(a));
  }

  set<GraphNode> result;
  set_intersection(visitedFW.begin(), visitedFW.end(),
                   visitedBW.begin(), visitedBW.end(),
                   inserter(result, result.begin()));
  return(result);
}

set<GraphNode> oneHop(const ListDigraph& graph,
                      const set<GraphNode>& from,
                      const set<GraphNode>& to,
                      bool forward,
                      set<GraphNode>& visited){
  set<GraphNode> result;

  queue<GraphNode> worklist;
  for(set<GraphNode>::const_iterator i = from.begin(), e = from.end(); i != e; ++i){
    if(forward){
      for(ListDigraph::OutArcIt a(graph, *i); a != INVALID; ++a)
        worklist.push(graph.target(a));
    }
    else{
      for(ListDigraph::InArcIt a(graph, *i); a != INVALID; ++a)
        worklist.push(graph.source(a));
    }
  }

  while(!worklist.empty()){
    GraphNode n = worklist.front();
    worklist.pop();
    if(visited.count(n))
      continue;
    else
      visited.insert(n);
    if(to.count(n)){
      result.insert(n);
      continue;
    }

    if(forward){
      for(ListDigraph::OutArcIt a(graph, n); a != INVALID; ++a)
        worklist.push(graph.target(a));
    }
    else{
      for(ListDigraph::InArcIt a(graph, n); a != INVALID; ++a)
        worklist.push(graph.source(a));
    }
  }

  return(result);
}

string setNodes_asstring(const set<GraphNode>& theSet,
                         const ListDigraph& graph){
  string result;
  for(set<GraphNode>::const_iterator i = theSet.begin(), e = theSet.end();
      i != e; ++i){
    if(i != theSet.begin())
      result += ',';
    result += graph.id(*i);
  }
  return(result);
}

// get a set of ambiguous triangles for an (alpha, beta, d) triple
// currently, this always returns either a single-element set or an empty set
// (based on whether at least one or no triangles exist)
set<LEMONtriangle> getAmbiguousTriangles(const ListDigraph& graph,
                                      const GraphNode& alpha,
                                      const GraphNode& beta,
                                      const GraphNode& d,
                                      const set<GraphNode>& X,
                                      const GraphNode& e,
                                      const ListDigraph::NodeMap<double>& S,
                                      map<GraphNode, set<GraphNode> >& y1Cache,
                                      map<GraphNode, set<GraphNode> >& y2Cache){
  assert(alpha != d && beta != d);

  int nodesInGraph = lemon::countNodes(graph);

  set<GraphNode> X_minus_d = X;
  X_minus_d.erase(d);

  set<GraphNode> Y1;
  const map<GraphNode, set<GraphNode> >::const_iterator alphaFound =
     y1Cache.find(alpha);
  if(alphaFound != y1Cache.end()){
    Y1 = alphaFound->second;
  }
  else{
    Y1 = connectedExcluding(graph,
                            set<GraphNode>(&e, &e+1),
                            set<GraphNode>(&alpha, &alpha+1),
                            set<GraphNode>(&d, &d+1));
    y1Cache[alpha] = Y1;
  }
  
  set<GraphNode> Y2;
  const map<GraphNode, set<GraphNode> >::const_iterator betaFound =
     y2Cache.find(beta);
  if(betaFound != y2Cache.end()){
    Y2 = betaFound->second;
  }
  else{
    Y2 = connectedExcluding(graph,
                            set<GraphNode>(&beta, &beta+1),
                            X_minus_d,
                            set<GraphNode>(&d, &d+1));
    y2Cache[beta] = Y2;
  }

  assert(!Y1.count(d) && !Y2.count(d));
  if(Y1.empty() || Y2.empty()){
    return(set<LEMONtriangle>());
  }
  assert(Y1.count(alpha) && Y2.count(beta));

  // we need the Y set explicitly to compute the symmetric difference
  set<GraphNode> Y;
  set_union(Y1.begin(), Y1.end(), Y2.begin(), Y2.end(), inserter(Y,Y.end()));

  // update weights in S based on computed Y set
  // (anything in Y changes to zero-weight)
  // all others get added to the arc weight map (needed for Dijkstra's) with
  // their original node weight on all incoming edges
  ListDigraph::ArcMap<double> weightMap(graph);
  // NOTE: ArcMap has no copy constructor (very mysterious) so we need to make
  // 2 of these, because one sets a high weight on crossing d: no d allowed on
  // the alpha->beta path of the triangle
  ListDigraph::ArcMap<double> noDWeightMap(graph);
  for(ListDigraph::ArcIt a(graph); a != INVALID; ++a){
    GraphNode targetNode = graph.target(a);
    if(Y.count(targetNode)){
      weightMap[a] = 0.0;
      noDWeightMap[a] = 0.0;
    }
    else{
      assert(S[targetNode] <= 1.0e-6 || S[targetNode] >= 1-1.0e-6);
      weightMap[a] = noDWeightMap[a] = S[targetNode] * nodesInGraph + 0.1;
    }

    if(targetNode == d)
      noDWeightMap[a] = 1.0 * nodesInGraph;
  }

  // find the cheapest (least-instrumented) triangle, based on the weight map
  Dijkstra<ListDigraph, ListDigraph::ArcMap<double> > djAlphaD(graph,
                                                               weightMap);
  Dijkstra<ListDigraph, ListDigraph::ArcMap<double> > djDBeta(graph,
                                                              weightMap);
  Dijkstra<ListDigraph, ListDigraph::ArcMap<double> > djAlphaBeta(graph,
                                                                  noDWeightMap);
  bool pathFound = djAlphaD.run(alpha, d);
  pathFound &= djDBeta.run(d, beta);
  pathFound &= djAlphaBeta.run(alpha, beta);

  set<LEMONtriangle> result;
  // NOTE: currently, if a node appears in both legs of the triangle,
  //       we count it twice for the path weight.  That is,
  //       the current implementation adds *all* instances of *each* node.
  //       If we only count each once, the shortest-path computation above gets
  //       much more complicated... 
  double pathWeight = djAlphaD.dist(d) +
                      djDBeta.dist(beta) +
                      djAlphaBeta.dist(beta);
  if(pathFound && pathWeight < 1.0 * nodesInGraph){
    LEMONtriangle theTriangle(pathWeight,
                              djAlphaD.path(d),
                              djDBeta.path(beta),
                              djAlphaBeta.path(beta),
                              Y,
                              graph);
    result.insert(theTriangle);
  }
  return(result);
}

// get a set of ambiguous triangles for a set of possible alphas and betas, and
// a single d
// currently, this always returns at most one triangle per (alpha, beta, d) triple
set<LEMONtriangle> getAmbiguousTriangles(const ListDigraph& graph,
                                      const set<GraphNode>& alphas,
                                      const set<GraphNode>& betas,
                                      const GraphNode& d,
                                      const set<GraphNode>& X,
                                      const GraphNode& e,
                                      const ListDigraph::NodeMap<double>& S,
				      unsigned int maxTriangles,
                                      map<GraphNode, set<GraphNode> >& y1Cache,
                                      map<GraphNode, set<GraphNode> >& y2Cache){
  if(maxTriangles == 0)
    maxTriangles = INT_MAX;

  set<LEMONtriangle> result;
  for(set<GraphNode>::const_iterator alpha = alphas.begin(),
                                        ae = alphas.end();
      alpha != ae; ++alpha){
    if(d == *alpha)
      continue;

    for(set<GraphNode>::const_iterator beta = betas.begin(),
                                         be = betas.end();
        beta != be; ++beta){
      if(d == *beta)
        continue;

      set<LEMONtriangle> ambTriangles = getAmbiguousTriangles(graph,
                                                              *alpha, *beta,
                                                              d, X, e, S,
                                                              y1Cache, y2Cache);
      if(!ambTriangles.empty()){
        DEBUG(dbgs() << "LEMON found a triangle: ("
                     << csi_inst::to_string(graph.id(*alpha)) << ", "
                     << csi_inst::to_string(graph.id(*beta)) << ", "
                     << csi_inst::to_string(graph.id(d)) << ")\n");
        result.insert(ambTriangles.begin(), ambTriangles.end());

        if(result.size() >= maxTriangles)
          goto end;
      }
    }
  }

end:
  return(result);
}

// Fill the alpha and beta sets based on the provided graph, entry node,
// crash points, and instrumentation (S) set
void fillAlphasBetas(const ListDigraph& graph,
	             const ListDigraph::NodeMap<double>& S,
	             const set<GraphNode>& X,
		     const GraphNode& e,
		     set<GraphNode>& alphas,
		     set<GraphNode>& betas){
  for(ListDigraph::NodeIt i(graph); i != INVALID; ++i){
    if(S[i] > 0.0){
      alphas.insert(i);
      betas.insert(i);
    }
  }
  alphas.insert(e);
  betas.insert(X.begin(), X.end());
}

set<LEMONtriangle> csi_inst::getTriangles(const ListDigraph& graph,
                                          const ListDigraph::NodeMap<double>& S,
                                          const set<GraphNode>& D,
                                          const set<GraphNode>& X,
                                          const GraphNode& e,
                                          unsigned int maxDistance,
					  unsigned int startDistance,
                                          unsigned int maxTriangles,
					  unsigned int maxTrianglesPerDistance){
  set<LEMONtriangle> result;

  set<GraphNode> alphas;
  set<GraphNode> betas;
  fillAlphasBetas(graph, S, X, e, alphas, betas);

  if(maxDistance == 0)
    maxDistance = INT_MAX;
  if(maxTriangles == 0)
    maxTriangles = INT_MAX;
  if(maxTrianglesPerDistance == 0)
    maxTrianglesPerDistance = INT_MAX;

  // iterate over D first to filter possible alphas and betas to consider, and
  // allow us to restrict alpha/beta distance
  for(set<GraphNode>::const_iterator d = D.begin(), de = D.end(); d != de; ++d){
    GraphNode thisD = *d;
    if(S[thisD] >= 1.0)
      continue;

    // triangles found for this d
    unsigned int trianglesForD = 0;

    // a cache so we don't need to re-compute Y sets
    map<GraphNode, set<GraphNode> > y1Cache;
    map<GraphNode, set<GraphNode> > y2Cache;

    // in both directions, we keep a "frontier" of last-step nodes from S, and
    // the set of all visited nodes
    // with this setup: the whole procedure is of the same complexity as a
    // standard breadth-first search
    set<GraphNode> myAlphas;
    set<GraphNode> alphaFrontier;
    set<GraphNode> alphaVisited;
    alphaFrontier.insert(thisD);
    alphaVisited.insert(thisD);
    set<GraphNode> myBetas;
    set<GraphNode> betaFrontier;
    set<GraphNode> betaVisited;
    betaFrontier.insert(thisD);
    betaVisited.insert(thisD);
    for(unsigned int i = 0;
        i < maxDistance && (!alphaFrontier.empty() || !betaFrontier.empty());
        ++i){
      set<LEMONtriangle> ambTriangles;

      alphaFrontier = oneHop(graph, alphaFrontier, alphas, false, alphaVisited);
      if(i+1 >= startDistance){
	unsigned int maxToFind = min(maxTrianglesPerDistance,
                                     maxTriangles-trianglesForD);
        ambTriangles = getAmbiguousTriangles(graph,
                                             alphaFrontier, myBetas,
                                             thisD, X, e, S,
	                                     maxToFind,
                                             y1Cache, y2Cache);
      }
      if(!ambTriangles.empty()){
	trianglesForD += ambTriangles.size();
        result.insert(ambTriangles.begin(), ambTriangles.end());
        if(trianglesForD >= maxTriangles)
          break;
      }
      myAlphas.insert(alphaFrontier.begin(), alphaFrontier.end());
      ambTriangles.clear();

      betaFrontier = oneHop(graph, betaFrontier, betas, true, betaVisited);
      if(i+1 >= startDistance){
	unsigned int maxToFind = min(maxTrianglesPerDistance,
                                     maxTriangles-trianglesForD);
        ambTriangles = getAmbiguousTriangles(graph, myAlphas,
                                             betaFrontier, thisD, X, e, S,
					     maxToFind,
                                             y1Cache, y2Cache);
      }
      if(!ambTriangles.empty()){
	trianglesForD += ambTriangles.size();
        result.insert(ambTriangles.begin(), ambTriangles.end());
        if(trianglesForD >= maxTriangles)
          break;
      }
      myBetas.insert(betaFrontier.begin(), betaFrontier.end());
    }
  }

  return(result);
}

unsigned int csi_inst::getMaxDistance(const ListDigraph& graph,
				      const ListDigraph::NodeMap<double>& S,
				      const set<GraphNode>& D,
				      const set<GraphNode>& X,
				      const GraphNode& e){
  unsigned int maxDepth = 0;

  set<GraphNode> alphas;
  set<GraphNode> betas;
  fillAlphasBetas(graph, S, X, e, alphas, betas);

  for(set<GraphNode>::const_iterator d = D.begin(), de = D.end(); d != de; ++d){
    GraphNode thisD = *d;
    if(S[thisD] >= 1.0)
      continue;

    // start at -1 because the frontier "stepping" loop below will iterate one
    // more time than the actual max distance
    unsigned int thisDepth = -1;

    // just like in getTriangles(), step forward and backward one
    // depth at a time for both alphas and betas
    set<GraphNode> alphaFrontier;
    set<GraphNode> alphaVisited;
    alphaFrontier.insert(thisD);
    alphaVisited.insert(thisD);
    set<GraphNode> betaFrontier;
    set<GraphNode> betaVisited;
    betaFrontier.insert(thisD);
    betaVisited.insert(thisD);
    for(unsigned int i = 0;
	!alphaFrontier.empty() || !betaFrontier.empty();
	++i){
      alphaFrontier = oneHop(graph, alphaFrontier, alphas, false, alphaVisited);
      betaFrontier = oneHop(graph, betaFrontier, betas, true, betaVisited);

      ++thisDepth;
    }

    maxDepth = max(maxDepth, thisDepth);
  }

  return(maxDepth);
}
