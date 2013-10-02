#====--------------------------- expectcmp.py -----------------------------====#
#
# This script defines classes for specific non-standard syntactic comparison of
# output files.
#====----------------------------------------------------------------------====#
#
# Copyright (c) 2013 Peter J. Ohmann and Benjamin R. Liblit
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

########################################################################
#
#  utility functions for comparison with reference output
#

def read_rt(f):
  try:
    fp = open(f, "r");
    
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
      
      # read the call-site entries for this function
      line = fp.readline();
      while(line):
        if(not line or not line[0].isdigit()):
          break;
        else:
          lineParts = [x.strip() for x in line.split('|')];
          if(len(lineParts) != 3):
            print >> stderr, ("ERROR 3: incorrect formatting in rt file " + f);
            exit(3);
          index = lineParts[0];
          lineNum = lineParts[1];
          calledFn = lineParts[2];
          funcData.addEntry((lineNum, calledFn));
          line = fp.readline();
        #end if
      #end while
      funcRts += [funcData];
    #end while
    
    return funcRts;
  except IOError:
    print >> stderr, ("ERROR: could not read from rt file " + f);
    exit(4);
#end: read_rt
