#====--------------------------- expectcmp.py -----------------------------====#
#
# This script defines classes for specific non-standard syntactic comparison of
# output files.
#====----------------------------------------------------------------------====#
#
# Copyright (c) 2023 Peter J. Ohmann and Benjamin R. Liblit
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#====----------------------------------------------------------------------====#
from sys import stderr
from itertools import groupby

# an "enum" denoting the types of local/global coverage data
class RTType:
  CC = 1
  BBC = 2
#end: RTType

class RTData:
  __slots__ = "_function", "_arrayName", "_entries";
  
  def __init__(self, function, arrayName):
    self._function = function;
    self._arrayName = arrayName;
    self._entries = [];
  #end: __init__
  
  def addEntry(self, entry):
    self._entries += [entry];
  #end: addEntry
  
  def __cmp__(self, other):
    if(not isinstance(other, RTData)):
      raise TypeError("Innapropriate comparison of " + str(other) + \
                      " to RTData");
    myValue = (self._function, self._arrayName, sorted(self._entries));
    otherValue = (other._function, other._arrayName, sorted(other._entries));
    if(myValue < otherValue):
      return(-1);
    elif(myValue > otherValue):
      return(1);
    else:
      return(0);
  #end: __cmp__
#end: RTData

class PTData:
  __slots__ = "_function", "_nodes", "_edges", "_entries", "_exits";
  
  def __init__(self, function):
    self._function = function;
    self._nodes = {};
    self._edges = set([]);
    self._entry = None;
    self._exits = set([]);
  #end: __init__
  
  def addNode(self, num, entries):
    if(num in self._nodes):
      raise KeyError("Node '" + str(num) + "' already exists for function '" + \
                     str(self._function) + "'");
    self._nodes[num] = entries;
  #end: addNode
  
  def addEdge(self, fromNode, toNode, isBackedge, increment, weight):
    edge = (fromNode, toNode, isBackedge, increment, weight);
    if(edge in self._edges):
      raise KeyError("Edge '" + str(fromNode) + "'->'" + str(toNode) + "' " + \
                     "already exists for function '" + str(self._function) + \
                     "'");
    self._edges.add(edge);
  #end: addEdge
  
  def setEntryNode(self, node):
    if(self._entry != None):
      raise ValueError("Multiple entry nodes given for function '" + \
                       str(self._function) + "'");
    self._entry = node;
  #end: addEntryNode
  
  def addExitNode(self, node):
    self._exits.add(node);
  #end: addExitNode
  
  def verify(self):
    nodeSet = set(self._nodes.keys());
    
    for (fromNode, toNode, isBackedge, inc, weight) in self._edges:
      if(fromNode not in nodeSet or toNode not in nodeSet):
        return False;
    #end for
    
    return ((self._entry in nodeSet) and (self._exits <= nodeSet));
  #end: verify
  
  def __cmp__(self, other):
    if(not isinstance(other, PTData)):
      raise TypeError("Innapropriate comparison of " + str(other) + \
                      " to PTData");
    myValue = (self._function, self._nodes, self._edges,
               self._entry, self._exits);
    otherValue = (other._function, other._nodes, other._edges,
                  other._entry, other._exits);
    if(myValue < otherValue):
      return(-1);
    elif(myValue > otherValue):
      return(1);
    else:
      return(0);
  #end: __cmp__
#end: PTData

########################################################################
#
#  utility functions for comparison with reference output
#

def package_cc_line_data(lineParts):
  if(len(lineParts) != 4):
    print >> stderr, ("ERROR: incorrect line formatting in cc file " + str(lineParts));
    exit(1);
  isInst = (lineParts[0][0] != '-');
  lineNum = lineParts[2];
  calledFn = lineParts[3].replace("__isoc99_scanf", "scanf");
  return((isInst, lineNum, calledFn));
#end: package_cc_line_data

def package_bbc_line_data(lineParts):
  if(len(lineParts) < 3):
    print >> stderr, ("ERROR: incorrect line formatting in bbc file " + str(lineParts));
    exit(1);
  isInst = (lineParts[0][0] != '-');
  if(len(lineParts) == 3 and lineParts[2].strip() == "NULL"):
    lineNums = [];
  else:
    lineNums = [b[0] for b in groupby(map(int, lineParts[2:]))];
  return(isInst, lineNums);
#end: package_bbc_line_data

def read_rt(f, rtType):
  with open(f, "r") as fp:
    try:
      funcRts = [];
      
      line = fp.readline();
      while True:
        if(not line):
          break;
        elif(not line.strip()):
          line = fp.readline();
          continue;
        elif(line[0] != '#'):
          print >> stderr, ("ERROR 1: incorrect formatting in rt file " + f);
          exit(1);
        
        lineParts = line[1:].strip().split('|');
        if(len(lineParts) != 2):
          print >> stderr, ("ERROR 2: incorrect formatting in rt file " + f);
          exit(2);
        
        funcData = RTData(lineParts[0], lineParts[1]);
        
        # read the data entries for this function
        uniqueIds = set([]);
        line = fp.readline();
        while(line):
          # id number is negative for uninstrumented sites
          isInstrumented = (line and line[0] != '-');

          if(not line or not (line[0].isdigit() or not isInstrumented)):
            break;
          else:
            lineParts = [x.strip() for x in line.split('|')];
            lineId = lineParts[1].strip();
            # ids are non-unique (or completely missing) for uninstrumented
            if(lineId in uniqueIds and isInstrumented):
              print >> stderr, ("ERROR: non-unique ID in file " + f);
              exit(1);
            else:
              uniqueIds.add(lineId);

            if(rtType == RTType.CC):
              data = package_cc_line_data(lineParts);
            elif(rtType == RTType.BBC):
              data = package_bbc_line_data(lineParts);
            else:
              print >> stderr, ("ERROR: invalid coverage type " + str(rtType));
              exit(1);
            funcData.addEntry(data);
            line = fp.readline();
          #end if
        #end while
        funcRts += [funcData];
      #end while
      
      return funcRts;
    except IOError:
      print >> stderr, ("ERROR: could not read from rt file " + f);
      exit(4);
    #end try
  #end with
#end: read_rt

def read_pt(f):
  with open(f, "r") as fp:
    try:
      funcCFGs = [];
      
      line = fp.readline();
      while True:
        if(not line):
          break;
        elif(line == "\n"):
          line = fp.readline();
          continue;
        elif(line != "#\n"):
          print >> stderr, ("ERROR 1: incorrect formatting in pt file " + f);
          exit(1);
        
        funcName = fp.readline();
        if(not funcName):
          print >> stderr, ("ERROR 2: incorrect formatting in pt file " + f);
          exit(2);
        
        cfg = PTData(funcName);
        
        # read the basic blocks for this function
        bbs = {};
        line = fp.readline();
        while(line):
          if(not line or not line[0].isdigit()):
            break;
          else:
            lineParts = [x.strip() for x in line.split("|")];
            nodeId = int(lineParts[0]);
            lineNums = lineParts[1:];
            
            if(len(lineNums) == 1 and (lineNums[0].strip() == "NULL" or \
                                       lineNums[0].strip() == "EXIT")):
              # add block with no line numbers
              cfg.addNode(nodeId, []);
              if(lineNums[0].strip() == "EXIT"):
                cfg.addExitNode(nodeId);
            else:
              # add block, removing consecutive duplicate line numbers
              if(lineNums[0] == "ENTRY"):
                cfg.setEntryNode(nodeId);
                cfg.addNode(nodeId,
                            [b[0] for b in groupby(map(int, lineNums[1:]))]);
              else:
                cfg.addNode(nodeId,
                            [b[0] for b in groupby(map(int, lineNums))]);
            #end if
            line = fp.readline();
          #end if
        #end while
        
        if(line != "$\n"):
          print >> stderr, ("ERROR 3: incorrect formatting in pt file " + f);
          exit(3);
        
        # read the cfg edges and increments
        line = fp.readline();
        while(line):
          if(not line or not line[0].isdigit()):
            break;
        
          # throw away end-of-line comments
          strippedLine = [x.strip() for x in line.split("#")][0];
          
          lineParts = [x.strip() for x in strippedLine.split("|")];
          if(len(lineParts) != 2):
            print >> stderr, ("ERROR 4: incorrect formatting in pt file " + f);
            exit(4);
          #end if
          
          isBackedge = False;
          edgeNodes = lineParts[0].split("~>");
          if(len(edgeNodes) == 2):
            isBackedge = True;
          else:
            edgeNodes = lineParts[0].split("->");
            if(len(edgeNodes) != 2):
              print >> stderr, ("ERROR 5: incorrect formatting in pt file " + \
                                f);
              exit(5);
          #end if
          
          edgeWeights = [x.strip() for x in lineParts[1].strip().split("$")];
          if(len(edgeWeights) != 2):
            print >> stderr, ("ERROR 6: incorrect formatting in pt file " + f);
            exit(6);
          
          cfg.addEdge(int(edgeNodes[0]), int(edgeNodes[1]), isBackedge,
                      int(edgeWeights[0]), int(edgeWeights[1]));
          line = fp.readline();
        #end while
        
        funcCFGs += [cfg];
      #end while
      
      return funcCFGs;
    except IOError:
      print >> stderr, ("ERROR: could not read from pt file " + f);
      exit(8);
    except ValueError, e:
      print >> stderr, ("ERROR: line number conversion error in pt file " + f);
      print >> stderr, (str(e));
      exit(9);
    #end try
  #end with
#end: read_pt
