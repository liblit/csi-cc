//===------------------------- LEMONCoverageSet.h -------------------------===//
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
#ifndef CSI_LEMON_COVERAGE_SET_H
#define CSI_LEMON_COVERAGE_SET_H

#include "lemon/list_graph.h"
#include <lemon/path.h>

#include <set>

namespace csi_inst {

class LEMONtriangle {
private:
  // the total weight of instrumentation in the triangle (always < 1.0)
  double totalWeight;
  // the set of nodes in the symmetric difference of the paths
  std::set<lemon::ListDigraph::Node> symDiff;

public:
  LEMONtriangle(double weight,
                const lemon::Path<lemon::ListDigraph>& alphaD,
                const lemon::Path<lemon::ListDigraph>& dBeta,
                const lemon::Path<lemon::ListDigraph>& alphaBeta,
                const std::set<lemon::ListDigraph::Node>& Y,
                const lemon::ListDigraph& graph);

  inline double getTotalWeight() const {
    return(totalWeight);
  }

  inline const std::set<lemon::ListDigraph::Node>& getSymmetricDifference() const {
    return(symDiff);
  }

  inline bool operator<(const LEMONtriangle& other) const {
    return(totalWeight < other.totalWeight ||
           (totalWeight == other.totalWeight && symDiff < other.symDiff));
  }
};

// determine if a particular set is a coverage set of the specified desired
// nodes, by returning a collection of ambiguous triangles.  Note that
// the number of entries returned is *some subset* of the ambiguous triangles
// in "graph".  It is *not* necessarily all triangles that exist.
// If maxDistance and startDistance are both 0 and the returned set is
// empty: S is a coverage set of D.
//
// Optional Arguments:
//   maxDistance: If non-zero, only consider the closest maxDistance alphas
//                and betas in the search.  A common value of 1 would be used
//                for seeding the optimizer.  If this parameter is given (and
//                is non-zero), an empty set result does *not* necessarily mean
//                that S is a coverage set of D.
//   startDistance: If non-zero, only consider alphas and betas at distances
//                  startDistance or more from each D.  This is mostly useful
//                  for "continuing the search" from a previous call that used
//                  maxDistance.
//   maxTriangles: Only return up to maxTriangles triangles per desired node.
//                 The default is 1.
//   maxTrianglesPerDistance: Only return up to maxTrianglesPerDistance
//                            triangles at each individual distance (i.e., hop)
//                            from each D.  The default is 1.
std::set<LEMONtriangle> getTriangles(const lemon::ListDigraph& graph,
                                const lemon::ListDigraph::NodeMap<double>& S,
                                const std::set<lemon::ListDigraph::Node>& D,
                                const std::set<lemon::ListDigraph::Node>& X,
                                const lemon::ListDigraph::Node& e,
                                unsigned int maxDistance = 0,
	                        unsigned int startDistance = 0,
                                unsigned int maxTriangles = 1,
				unsigned int maxTrianglesPerDistance = 1);

// determine all nodes reachable along any path from a node in "from" to a
// node in "to" without passing through any nodes in "excluding"
std::set<lemon::ListDigraph::Node> connectedExcluding(
     const lemon::ListDigraph& graph,
     const std::set<lemon::ListDigraph::Node>& from,
     const std::set<lemon::ListDigraph::Node>& to,
     const std::set<lemon::ListDigraph::Node>& excluding);

unsigned int getMaxDistance(const lemon::ListDigraph& graph,
	        	    const lemon::ListDigraph::NodeMap<double>& S,
			    const std::set<lemon::ListDigraph::Node>& D,
			    const std::set<lemon::ListDigraph::Node>& X,
			    const lemon::ListDigraph::Node& e);

} // end csi_inst namespace

#endif
