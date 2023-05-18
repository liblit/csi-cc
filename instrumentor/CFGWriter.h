//===-------------------------- CFGWriter.h -----------------------------===//
//
// This pass prints the module's CFG to an output file in graphml format.
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
//
//                      The LLVM Compiler Infrastructure
//
// This file is derived from the file PathProfiling.cpp, originally distributed
// with the LLVM Compiler Infrastructure, originally licensed under the
// University of Illinois Open Source License.  See UI_LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//
#ifndef CSI_CFG_WRITER_H
#define CSI_CFG_WRITER_H

#include "PassName.h"

#include <llvm/Pass.h>

#include "llvm_proxy/CFG.h"

#include <fstream>
#include <map>
#include <set>

namespace csi_inst {

// ---------------------------------------------------------------------------
// CFGWriter is a module pass that writes out the module's interprocedural
// CFG in an appropriate format for CSI anlysis
// ---------------------------------------------------------------------------
class CFGWriter : public llvm::ModulePass {
private:
  // the stream to the output file (for writing the CFG)
  std::ofstream outputStream;

  // the module's indirect targets (complete assuming whole-program bitcode)
  std::set<llvm::Function*> indirectTargets;

  // a mapping from functions to their (negative) integer ID used to identify
  // nodes from that function (following the expected style for CSI analysis
  // graphs)
  std::map<const llvm::Function*, int> functionToIdMap;

  // given node identifiers, produce the appropriate node name
  std::string nodeNameFromData(int functionId, unsigned int nodeId);

  // appropriately-formatted node data writer (for expected graphml format)
  void writeDataIfNonEmpty(const std::string& type, const std::string& value);

  // write out edge data in appropriate graphml format
  void writeEdge(const std::string& from, const std::string& to,
                 const std::string& type="", const std::string& scope="");

  // write out node data, explicitly providing all node information
  void writeNode(int functionId, unsigned int nodeId,
                 const std::string& label="", const std::string& block="",
                 const std::string& kind="", const std::string& lines="",
                 const std::string& csiLabel="", const std::string& callName="",
                 const std::string& file="", const std::string& procedure="");

  // write out node data based on an LLVM instruction (convenience wrapper for
  // "writeNode()")
  void writeNodeFromInstruction(const llvm::Instruction& instruction,
                                int functionId,
                                unsigned int nodeId);

  // write necessary preamble/postamble for graphml output
  void writePreamble();
  void writePostamble();

  // Write out a single function's CFG
  bool runOnFunction(llvm::Function &F);
  // Perform module-level tasks, open streams, find interprocedural call
  // targets, and instrument each function
  bool runOnModule(llvm::Module &M);

public:
  static char ID; // Pass identification, replacement for typeid
  CFGWriter() : ModulePass(ID) {}

  virtual PassName getPassName() const {
    return "CSI Control-Flow Graph Writer";
  }

  void getAnalysisUsage(llvm::AnalysisUsage &) const;
};
} // end csi_inst namespace

#endif
