//===---------------------- InstrumentationData.cpp -----------------------===//
//
// This module contains utilities for CSI instrumentation preparation.
//
//===----------------------------------------------------------------------===//
//
// Copyright (c) 2016 Peter J. Ohmann and Benjamin R. Liblit
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
#include "BBCoverage.h"
#include "CallCoverage.h"
#include "CoveragePassNames.h"
#include "FuncCoverage.h"
#include "InstrumentationData.h"

using namespace csi_inst;
using namespace llvm;
using namespace std;

static bool hasCallsFilter(const set<string>& scheme, Function* F){
  if(!scheme.count("CC"))
    return(true);

  const ExtrinsicCalls<inst_iterator> calls = extrinsicCalls(*F);
  return(calls.begin() != calls.end());
}

static bool coverageFilter(const set<string>& scheme, Function*){
  bool hasCC = scheme.count(CallCoverage::names.upperShort);
  bool hasFC = scheme.count(FuncCoverage::names.upperShort);
  bool hasBBC = scheme.count(BBCoverage::names.upperShort);
  if(hasBBC && (hasFC || hasCC)) return(false);
  return(true);
}

static const FilterFn csi_filters[] = {coverageFilter, hasCallsFilter};
const vector<FilterFn> csi_inst::Filters(csi_filters,
					 csi_filters + sizeof(csi_filters) /
					 sizeof(csi_filters[0]));

static const string csi_inst_arr[] = {
  BBCoverage::names.upperShort,
  CallCoverage::names.upperShort,
  FuncCoverage::names.upperShort,
  "PT"
};
const set<string> csi_inst::Instrumentors(csi_inst_arr,
					  csi_inst_arr + sizeof(csi_inst_arr) /
					  sizeof(csi_inst_arr[0]));
