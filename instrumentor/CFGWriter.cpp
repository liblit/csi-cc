//===------------------------- CFGWriter.cpp ----------------------------===//
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
#define DEBUG_TYPE "cfg-writer"

#include "CFGWriter.h"
#include "Utils.hpp"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>

#include "Versions.h"
#include "llvm_proxy/CommandLine.h"
#include "llvm_proxy/DebugInfo.h"
#include "llvm_proxy/Module.h"
#include "llvm_proxy/InstIterator.h"
#include "llvm_proxy/IntrinsicInst.h"

using namespace csi_inst;
using namespace llvm;
using namespace std;

#define ENTRY_ID 1
#define EXIT_ID 2
#define FIRST_ID 3

static cl::opt<bool> SilentInternal("cfg-silent", cl::desc("Silence internal "
                                    "warnings.  Will still print errors which "
                                    "cause CFG writing to fail."));

static cl::opt<string> OutputFile("cfg-file", cl::desc("The path to the CFG "
                                  "output file."), cl::value_desc("file_path"));

// Register CFG writer as a pass
char CFGWriter::ID = 0;
static RegisterPass<CFGWriter> X("cfg-write",
                "Write out the CFG in appropriate format for CSI analysis",
                true, true);


string CFGWriter::nodeNameFromData(int functionId, unsigned int nodeId){
  return(string("n:") + csi_inst::to_string(functionId) + ':' +
         csi_inst::to_string(nodeId));
}

void CFGWriter::writeDataIfNonEmpty(const string& type, const string& value){
  assert(!type.empty());
  if(!value.empty())
    outputStream << "      <data key=\"" << type << "\">" << value << "</data>\n";
}

void CFGWriter::writeEdge(const string& from, const string& to,
                          const string& type, const string& scope){
  assert(!from.empty());
  assert(!to.empty());
  outputStream << "    <edge source=\"" << from << "\" target=\"" << to << "\"";
  if(type.empty() && scope.empty()){
    outputStream << "/>\n";
  }
  else{
    outputStream << ">\n";
    writeDataIfNonEmpty("type", type);
    writeDataIfNonEmpty("scope", scope);
    outputStream << "    </edge>\n";
  }
}

void CFGWriter::writeNode(int functionId, unsigned int nodeId,
                          const string& label, const string& block,
                          const string& kind, const string& lines,
                          const string& csiLabel, const string& callName,
                          const string& file, const string& procedure){
  string nodeName = nodeNameFromData(functionId, nodeId);
  outputStream << "    <node id=\"" << nodeName << "\">\n";

  // TODO: possibly remove this at some point.  For now, we expect all bbs to
  // have names so we can better match trace data
  // NOTE: fails for gcc's CFG (far into printing...I'm not sure which function)
  //assert(!block.empty());

  writeDataIfNonEmpty("label", label);
  writeDataIfNonEmpty("basic-block", block);
  writeDataIfNonEmpty("kind", kind);
  writeDataIfNonEmpty("lines", lines);
  writeDataIfNonEmpty("csi-label", csiLabel);
  writeDataIfNonEmpty("call-name", callName);
  writeDataIfNonEmpty("file", file);
  writeDataIfNonEmpty("procedure", procedure);

  outputStream << "    </node>\n";
}

// A deeper version of LLVM's getCalledFunction() that will "unwrap" bitcasts
// of a called function, allowing us to identify more direct calls that would
// normally look like indirect calls to a single possible target
static const Function* getUnwrappedCalledFunction(const CallInst& instruction){
  const Function* calledFn = instruction.getCalledFunction();

  // check for actual indirect calls (after linking, some external calls
  // become direct, and we want to match these where possible)
  if(calledFn == NULL){
    // NOTE: the following appears valid for LLVM 3.5 and earlier, but
    // may be much simpler starting with 3.6 (see class BitCastOperator)
    const ConstantExpr* constExpr =
       dyn_cast<ConstantExpr>(instruction.getCalledValue());
    if(constExpr && constExpr->isCast() &&
       constExpr->getOpcode() == Instruction::BitCast){
      assert(constExpr->getNumOperands() == 1);

      const Function* wrappedFn = dyn_cast<Function>(constExpr->getOperand(0));
      if(wrappedFn)
        calledFn = wrappedFn;
    }
  }

  return(calledFn);
}

// extract line data for the provided instruction.  If the instruction has no
// line data, return an empty string.
string lineFromInstruction(const Instruction& instruction){
  DebugLoc dbLoc = instruction.getDebugLoc();
#if LLVM_VERSION < 30700
  if(!dbLoc.isUnknown())
#else
  if(dbLoc)
#endif
    return(csi_inst::to_string(dbLoc.getLine()));
  else
    return("");
}

// extract CSI label metadata for the provided instruction.  If the instruction
// has no such metadata, return an empty string.
string csiLabelFromInstruction(const Instruction& instruction){
  MDNode* labelMD = instruction.getMetadata("CSI.label");
  if(labelMD){
    assert(labelMD->getNumOperands() == 1);
    MDString* labelString = dyn_cast<MDString>(labelMD->getOperand(0));
    assert(labelString);
    return(labelString->getString().str());
  }
  else{
    return("");
  }
}

void CFGWriter::writeNodeFromInstruction(const Instruction& instruction,
                                         int functionId,
                                         unsigned int nodeId){
  // get the full instruction text as the initial label
  string label;
  raw_string_ostream labelStream(label);
  instruction.print(labelStream);

  // then trim label down to just the instruction name
  size_t sizeToCopy = 0;
  while(sizeToCopy < label.size() && isspace(label[sizeToCopy]))
    sizeToCopy++;
  while(sizeToCopy < label.size() && !isspace(label[sizeToCopy]))
    sizeToCopy++;
  label = label.substr(0, sizeToCopy);

  string block = instruction.getParent()->getName().str();

  string kind = "expression";
  string callName = "";
  const CallInst* callInst = dyn_cast<CallInst>(&instruction);
  if(callInst){
    const Function* calledFn = getUnwrappedCalledFunction(*callInst);
    if(!calledFn || !calledFn->isIntrinsic()){
      kind = "call-site";
      if(calledFn)
        callName = calledFn->getName().str();
    }
  }

  string lines = lineFromInstruction(instruction);
  if(!lines.empty())
    lines = string("(") + lines + ')';

  string csiLabel = csiLabelFromInstruction(instruction);

  writeNode(functionId, nodeId, label, block, kind, lines, csiLabel, callName);
}


// An instruction is "useful" (and should be written into the graphml) if any
// of the following conditions hold:
// (1) It has a line number
// (2) It has a CSI label
// (3) It is a call
// (4) It is a terminator
bool isUsefulCFGInstruction(const Instruction* i){
  return(!lineFromInstruction(*i).empty() ||
         !csiLabelFromInstruction(*i).empty() ||
         dyn_cast<CallInst>(i) ||
         dyn_cast<TerminatorInst>(i));
}

string strFromValue(const Type* v){
  string result;
  raw_string_ostream streamer(result);
  v->print(streamer);
  return(result);
}

/**
 * This pulls out some of the repeated code, but hopefully we can do better
 * someday and actually return the DISubprogram itself (though the type--pointer
 * versus reference--seems to differ in earlier LLVM versions).
 */
const MDNode* functionToScopeMetadata(Function& F){
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i){
    const DebugLoc& dbLoc = i->getDebugLoc();
#if LLVM_VERSION < 30700
    if(!dbLoc.isUnknown())
#else
    if(dbLoc)
#endif
    {
#if LLVM_VERSION < 30700
      MDNode* instrScope = dbLoc.getScope(F.getContext());
      assert(instrScope);
      DISubprogram fnMetadata(instrScope);
#else
      MDNode* instrScope = dbLoc.getScope();
      assert(instrScope);
      const DISubprogram * const spPtr { getDISubprogram(instrScope) };
      const DISubprogram& fnMetadata = *spPtr;
#endif
      if(fnMetadata.describes(&F))
        return instrScope;
    }
  }
  return NULL;
}

string fileForFunction(Function& F){
  const MDNode* const scopeForF = functionToScopeMetadata(F);
  if(scopeForF == NULL){
    return string();
  }
  else{
    assert(scopeForF);
#if LLVM_VERSION < 30700
    DISubprogram fnMetadata(scopeForF);
#else
    const DISubprogram * const spPtr { getDISubprogram(scopeForF) };
    const DISubprogram& fnMetadata = *spPtr;
#endif
    if(!fnMetadata.describes(&F)){
      // this is actually a serious return value error, so we'll have to print a
      // cryptic message, since we're talking debug data here
      report_fatal_error("internal error: function metadata both does and does "
                         "not describe function " + F.getName().str());
    }
    string fileName = fnMetadata.getFilename().str();
    if(!fileName.empty() && fileName.at(0) == '/')
      return(fileName);
    else
      return(fnMetadata.getDirectory().str() + '/' + fileName);
  }
}

unsigned lineForFunction(Function& F){
  const MDNode* const scopeForF = functionToScopeMetadata(F);
  if(scopeForF == NULL){
    return 0;
  }
  else{
    assert(scopeForF);
#if LLVM_VERSION < 30700
    DISubprogram fnMetadata(scopeForF);
#else
    const DISubprogram * const spPtr { getDISubprogram(scopeForF) };
    const DISubprogram& fnMetadata = *spPtr;
#endif
    if(!fnMetadata.describes(&F)){
      // this is actually a serious return value error, so we'll have to print a
      // cryptic message, since we're talking debug data here
      report_fatal_error("internal error: function metadata both does and does "
                         "not describe function " + F.getName().str());
    }
    return fnMetadata.getLine();
  }
}


bool CFGWriter::runOnFunction(Function &F) {
  int functionId = functionToIdMap.at(&F);

  // XML comment line (for easier searching in the CFG text)
  outputStream << "    <!-- cfg " << functionId << ' '
               << F.getName().str() << " -->\n";

  // use debug data to find the file and line number containing this function
  string containingFile = fileForFunction(F);
  unsigned functionLine = lineForFunction(F);
  // this was once an assertion: violated by the following set of functions in
  // source file "config.h" of the "gcc" subject:
  // const_section, tdesc_section, sdata_section, ctors_section, dtors_section
  if(containingFile.empty() && F.begin() != F.end()){
    if(!SilentInternal)
      dbgs() << "No file information found for function '" << F.getName().str()
             << "'\n";
  }

  // create entry and exit nodes (edges to/from created later)
  writeNode(functionId, ENTRY_ID, "entry: " + F.getName().str(),
            "function_entry", "entry", "( " + to_string(functionLine) + " )",
            "", "", containingFile, F.getName().str());
  writeNode(functionId, EXIT_ID, "exit: " + F.getName().str(),
            "function_exit", "exit", "( 0 )");

  // for function declarations (hopefully just libc at this point), we still
  // want an empty body...just in case they are used in function pointers
  if(F.isDeclaration()){
    writeEdge(nodeNameFromData(functionId, ENTRY_ID),
              nodeNameFromData(functionId, EXIT_ID));
    return(false);
  }

  // the first un-claimed node ID
  // skipping: 1 (entry) and 2 (exit)
  unsigned int nextId = FIRST_ID;

  // first, give IDs to the starts of each basic block (to simplify jumps later)
  map<const BasicBlock*, unsigned int> blockToEntryIdMap;
  for(Function::const_iterator bb = F.begin(), bb_e = F.end(); bb != bb_e; ++bb)
    blockToEntryIdMap[&*bb] = nextId++;

  // then, actually iterate over all basic blocks, and write out all
  // instructions
  for(Function::const_iterator bb = F.begin(), bb_e = F.end(); bb != bb_e; ++bb){
    string firstNodeName = nodeNameFromData(functionId, blockToEntryIdMap[&*bb]);

    // special handling for entry block (create extra edge from the function's
    // entry node)
    if(&*bb == &F.getEntryBlock())
      writeEdge(nodeNameFromData(functionId, ENTRY_ID), firstNodeName);

    bool firstNode = true;
    bool lastWasSkipped = false;
    for(BasicBlock::const_iterator i = bb->begin(), e = bb->end(); i != e; ++i){
      // non-call, non-terminator nodes with no debug information provide no
      // matching information for analysis: don't even write them out
      if(!isUsefulCFGInstruction(&*i)){
        lastWasSkipped = true;
        continue;
      }
      else{
        lastWasSkipped = false;
      }

      unsigned int nodeId = firstNode ? blockToEntryIdMap.at(&*bb) : nextId++;
      firstNode = false;
      string nodeName = nodeNameFromData(functionId, nodeId);

      writeNodeFromInstruction(*i, functionId, nodeId);

      const TerminatorInst* termInst = dyn_cast<TerminatorInst>(&*i);
      if(termInst){
        const ReturnInst* retInst = dyn_cast<ReturnInst>(termInst);
        // return instructions need special edges to the function's unique
        // exit node (expected by CSI analysis right now)
        if(retInst){
          assert(retInst->getNumSuccessors() == 0);
          writeEdge(nodeName, nodeNameFromData(functionId, EXIT_ID));
          continue;
        }

        // otherwise, for other bb terminators, add edges to successor blocks
        for(succ_const_iterator succBlock = succ_begin(&*bb),
                                succ_e = succ_end(&*bb);
            succBlock != succ_e; ++succBlock){
          string targetFirstNodeName = nodeNameFromData(functionId,
                                          blockToEntryIdMap.at(*succBlock));
          writeEdge(nodeName, targetFirstNodeName);
        }
      }
      else{
        // otherwise, we'll need an edge to the next instruction's node (not
        // yet created)
        string nextNodeName = nodeNameFromData(functionId, nextId);
        writeEdge(nodeName, nextNodeName);

        // add special interprocedural edge(s) for call instructions
        const CallInst* callInst = dyn_cast<CallInst>(&*i);
        if(callInst){
          const Function* calledFn = getUnwrappedCalledFunction(*callInst);
          bool isIndirect = (calledFn == NULL);
          bool isIntrinsic = (!isIndirect && calledFn->isIntrinsic());

          if(!isIndirect && !isIntrinsic){
            writeEdge(nodeName,
                      nodeNameFromData(functionToIdMap.at(calledFn), ENTRY_ID),
                      "control", "interprocedural");
          }
          else if(!isIntrinsic){
            // right now, we pessimistically assume all indirect calls can call
            // any function that has its address taken
            for(set<Function*>::const_iterator indirF = indirectTargets.begin(),
                                               indir_e = indirectTargets.end();
                indirF != indir_e; ++indirF){
              writeEdge(nodeName,
                        nodeNameFromData(functionToIdMap.at(*indirF), ENTRY_ID),
                        "control", "interprocedural");
            }
          }
        }
      }
    }

    if(firstNode || lastWasSkipped)
      report_fatal_error("internal error: no useful CFG nodes ending basic "
                         "block " + bb->getName().str());
  }

  return false;
}

void CFGWriter::writePreamble(){
  outputStream <<

"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\"\n"
"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
"    xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n"
"  <key id=\"nature\"      for=\"graph\" attr.name=\"nature\"     attr.type=\"string\"/>\n"
"  <key id=\"label\"       for=\"node\" attr.name=\"label\"       attr.type=\"string\"/>\n"
"  <key id=\"kind\"        for=\"node\" attr.name=\"kind\"        attr.type=\"string\"> <default>declaration</default> </key>\n"
"  <key id=\"syntax\"      for=\"node\" attr.name=\"syntax\"      attr.type=\"string\"/>\n"
"  <key id=\"basic-block\" for=\"node\" attr.name=\"basic-block\" attr.type=\"string\"/>\n"
"  <key id=\"file\"        for=\"node\" attr.name=\"file\"        attr.type=\"string\"/>\n"
"  <key id=\"procedure\"   for=\"node\" attr.name=\"procedure\"   attr.type=\"string\"/>\n"
"  <key id=\"lines\"       for=\"node\" attr.name=\"lines\"       attr.type=\"string\"/>\n"
"  <key id=\"call-id\"     for=\"node\" attr.name=\"call-id\"     attr.type=\"string\"/>\n"
"  <key id=\"call-name\"   for=\"node\" attr.name=\"call-name\"   attr.type=\"string\"/>\n"
"  <key id=\"alocs-used\"  for=\"node\" attr.name=\"alocs-used\"  attr.type=\"string\"/>\n"
"  <key id=\"alocs-defd\"  for=\"node\" attr.name=\"alocs-defd\"  attr.type=\"string\"/>\n"
"  <key id=\"alocs-mayd\"  for=\"node\" attr.name=\"alocs-mayd\"  attr.type=\"string\"/>\n"
"  <key id=\"csi-label\"   for=\"node\" attr.name=\"csi-label\"   attr.type=\"string\"/>\n"
"  <key id=\"type\"        for=\"edge\" attr.name=\"type\"        attr.type=\"string\"> <default>flow</default> </key>\n"
"  <key id=\"when\"        for=\"edge\" attr.name=\"when\"        attr.type=\"string\"> <default>true</default> </key>\n"
"  <key id=\"scope\"       for=\"edge\" attr.name=\"scope\"       attr.type=\"string\"> <default>intraprocedural</default> </key>\n"
"  <graph id=\"CFG\" edgedefault=\"directed\">\n"
"    <data key=\"nature\">CFG</data>\n"
  ;
}

void CFGWriter::writePostamble(){
  outputStream <<

  "  </graph>\n"
  "</graphml>\n"

  ;
}

bool CFGWriter::runOnModule(Module& M){
  static bool runBefore = false;
  if(runBefore)
    return(false);
  runBefore = true;

  // possible indirect targets...I think that this is complete assuming we
  // have whole-program bitcode?  Without that, I think that we would also need
  // to add all externally-visible functions, which would be terrible
  indirectTargets.clear();
  for(Module::iterator F = M.begin(), e = M.end(); F != e; ++F){
    if(F->hasAddressTaken())
      indirectTargets.insert(&*F);
  }

  if(OutputFile.empty())
    report_fatal_error("CFG Writer cannot continue: "
                       "-cfg-file [file] is required", false);
  outputStream.open(OutputFile.c_str(), ios::out | ios::trunc);
  if(!outputStream.is_open())
    report_fatal_error("unable to open cfg-file location: " + OutputFile);
  DEBUG(dbgs() << "Output stream opened to " << OutputFile << '\n');

  // write the graphml preamble, containing general stuff common to all CSI
  // graphml files
  writePreamble();

  // first, assign each function an id (to simplify finding the entry nodes for
  // calls to each function later)
  functionToIdMap.clear();
  int functionId = -1;
  for(Module::iterator i = M.begin(), e = M.end(); i != e; ++i){
    if(!i->isIntrinsic())
      functionToIdMap[&*i] = functionId--;
  }

  // then actually print out the graph data for each function
  for(Module::iterator i = M.begin(), e = M.end(); i != e; ++i){
    if(i->isIntrinsic())
      continue;
    runOnFunction(*i);
  }

  writePostamble();

  outputStream.close();
  return false;
}

void CFGWriter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}
