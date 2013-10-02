#!/usr/bin/python

#====--------------------------- csi-driver.py ----------------------------====#
#
# This script forms the front-end of the csi-cc instrumenting compiler.  It is
# built as a subclass of Driver, a gcc-emulating front end for LLVM-based
# instrumenting compilation.
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

from distutils.util import strtobool
from itertools import chain
from sys import argv, path, stderr
from os import devnull, environ
import os.path

PATH_TO_CSI = os.path.dirname(os.path.dirname(os.path.realpath(os.path.abspath(argv[0]))))
PATH_TO_CSI_RELEASE = os.path.join(PATH_TO_CSI, "Release")
PATH_TO_CSI_DRIVER = os.path.join(PATH_TO_CSI, "driver")

path.insert(1, PATH_TO_CSI_DRIVER)
from driver import Driver, drive

class CSIDriver(Driver):
  __slots__ = "__pathArraySize", "__hashSize", "__silent",\
              "__doPaths", "__doCalls",\
              "__trackerFile", "__ccFile"
  
  __pychecker__ = 'unusednames=_flag'
  
  def __handlePathArraySize(self, _flag, _value):
    self.__pathArraySize = _value
  
  def __handleHashSize(self, _flag, _value):
    self.__hashSize = _value
  
  def __handleSilent(self, _flag):
    self.__silent = True
  
  def __handleNoPathTrace(self, _flag):
    self.__doPaths = False
  
  def __handleNoCallCoverage(self, _flag):
    self.__doCalls = False
  
  def __handleFlagGoalHelpCSI(self, _flag):
    self.finalGoal = self.__buildCSIHelp
  
  def __handleFlagGoalHelpClang(self, _flag):
    return super(CSIDriver, self).handleFlagGoalHelp('--help')

  EXTRA_EXACT_HANDLERS = {
    "-path-array-size"   : __handlePathArraySize,
    "-hash-size"         : __handleHashSize,
    "--silent"           : __handleSilent,
    "--no-path-trace"    : __handleNoPathTrace,
    "--no-call-coverage" : __handleNoCallCoverage,
    "--help"             : __handleFlagGoalHelpCSI,
    "--help-clang"       : __handleFlagGoalHelpClang,
  }
  
  def __init__(self):
    Driver.__init__(self, extraExact=self.EXTRA_EXACT_HANDLERS)
    self.__pathArraySize = environ.get("PT_ARRAY_SIZE", "")
    self.__hashSize = environ.get("PT_HASH_SIZE", "")
    self.__silent = strtobool(environ.get("CSI_SILENT", "0").strip())
    self.__doPaths = strtobool(environ.get("CSI_PT", "1").strip())
    self.__doCalls = strtobool(environ.get("CSI_CC", "1").strip())
  
  def process(self, args):
    # instrumentation *requires* debug information
    return super(CSIDriver, self).process(['-g'] + args)

  def __checkPositiveInt(self, setting, flag, description):
    if setting:
      try:
        value = int(setting, 0)
      except ValueError:
        value = None
      if value > 0:
        yield flag
        yield str(value)
      else:
        print >>stderr, 'WARNING: %s must be a positive integer; ignoring "%s"' % (description, setting)

  def getExtraOptArgs(self):

    # instrumentation plug-in
    yield "-load"
    yield os.path.join(PATH_TO_CSI_RELEASE, "libCSI.so")

    # path tracing instrumentation
    if self.__doPaths:
      yield "-pt-inst"
      yield "-pt-tracker-file"
      yield self.__trackerFile
      for arg in self.__checkPositiveInt(self.__pathArraySize, '-pt-path-array-size', 'path tracing array size'):
        yield arg
      for arg in self.__checkPositiveInt(self.__hashSize, '-pt-hash-size', 'path count "hash" size'):
        yield arg
      if self.__silent:
        yield "-pt-silent"
    
    # call coverage instrumentation
    if(self.__doCalls):
      yield "-call-coverage"
      yield "-cc-info-file"
      yield self.__ccFile
    if(self.__silent):
      yield "-cc-silent"

  def instrumentBitcode(self, inputFile, uninstrumented, instrumented):
    # output files for static data
    self.__trackerFile = self.temporaryFile(inputFile, ".tracker.info")
    self.__ccFile = self.temporaryFile(inputFile, ".cc.info")

    super(CSIDriver, self).instrumentBitcode(inputFile, uninstrumented, instrumented)

  def __embedSection(self, objectFile, section, filename):
    if not os.path.exists(filename): return
    if os.path.getsize(filename) == 0: return
    self.run(('objcopy', '--add-section', '.debug_%s=%s' % (section, filename), objectFile))
  
  def compileTo(self, inputFile, objectFile, args, targetFlag):
    super(CSIDriver, self).compileTo(inputFile, objectFile, args, targetFlag)
    self.__embedSection(objectFile, 'PT', self.__trackerFile)
    self.__embedSection(objectFile, 'CC', self.__ccFile)

  def __buildCSIHelp(self, args):
    __pychecker__ = 'unusednames=args'
    print \
"""
OVERVIEW: csi-cc instrumenting LLVM compiler

USAGE: csi-cc [options] <inputs>

OPTIONS:
  -path-array-size <arg>  Use <arg> as the size of path tracing arrays
                          (Default: 10)
  -hash-size <arg>        Use <arg> as the maximum-size function (in number of
                          acyclic paths) to instrument for path tracing
                          (Default: ULONG_MAX/2+1)
  --silent                Do not print pass-specific warnings during
                          instrumentation
  --no-path-trace         Do not instrument for path tracing
  --no-call-coverage      Do not instrument for call coverage
  
  --help                  Display this help message and exit
  --help-clang            Display additional options (clang's help message) and
                          exit

ENVIRONMENT VARIABLES:
  PT_INST_FUNCS           A | separated list of functions for which path tracing
                          should be initially enabled.  The flag --no-path-trace
                          (above) has precedence, and nullifies any information
                          in this variable. If this environment variable does
                          not exist, is empty, or is set to 'ALL', all functions
                          are enabled.
  
  PT_ARRAY_SIZE           See -path-array-size (above).  Flags have precedence.
  PT_HASH_SIZE            See -hash-size (above).  Flags have precedence.
  CSI_SILENT              Enables or disables the printing of instrumentation
                          warnings.
                          See --silent (above).  Flags have precedence.
  CSI_PT                  Enables or disables path tracing instrumentation.
                          See --no-path-trace (above).  Flags have precedence.
  CSI_CC                  Enables or disables call coverage instrumentation.
                          See --no-call-coverage (above). Flags have precedence.
"""

def main():
  drive(CSIDriver())

if __name__ == '__main__':
  main()
