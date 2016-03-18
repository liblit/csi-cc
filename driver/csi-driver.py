#!/usr/bin/python

#====--------------------------- csi-driver.py ----------------------------====#
#
# This script forms the front-end of the csi-cc instrumenting compiler.  It is
# built as a subclass of Driver, a gcc-emulating front end for LLVM-based
# instrumenting compilation.
#====----------------------------------------------------------------------====#
#
# Copyright (c) 2016 Peter J. Ohmann and Benjamin R. Liblit
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
from driver import Driver, drive, regexpHandlerTable

class CSIDriver(Driver):
  __slots__ = "__pathArraySize", "__hashSize", "__silent",\
              "__ptFile", "__ccFile", "__bbcFile",\
              "__fcFile", "__traceFile",\
              "__debugPass", "__filter"
  
  __pychecker__ = 'unusednames=_flag'
  
  def __handlePathArraySize(self, _flag, _value):
    self.__pathArraySize = _value
  
  def __handleHashSize(self, _flag, _value):
    self.__hashSize = _value
  
  def __handleSilent(self, _flag):
    self.__silent = True
  
  def __handleFlagGoalHelpCSI(self, _flag):
    self.finalGoal = self.__buildCSIHelp
  
  def __handleFlagGoalHelpClang(self, _flag):
    return super(CSIDriver, self).handleFlagGoalHelp('--help')
  
  def __handleNoFilter(self, _flag):
    self.__filter = False
  
  def __handleSpecificTrace(self, _flag):
    if not os.path.exists(_flag[8:].strip()):
      print >> stderr, "ERROR: trace file does not exist.  Revise --trace argument."
      exit(1)
    else:
      self.__traceFile = _flag[8:].strip()
  
  def __handleDebugPass(self, _flag):
    self.__debugPass = _flag[12:].strip().lower()

  EXTRA_EXACT_HANDLERS = {
    "-path-array-size"   : __handlePathArraySize,
    "-hash-size"         : __handleHashSize,
    "-no-filter"         : __handleNoFilter,
    "--silent"           : __handleSilent,
    "--help"             : __handleFlagGoalHelpCSI,
    "--help-clang"       : __handleFlagGoalHelpClang
  }
  
  EXTRA_REGEXP_HANDLERS = regexpHandlerTable(
    ('^(--trace=.+)$', __handleSpecificTrace),
    ('^(-debug-pass=.+)$', __handleDebugPass),
  )
  
  def __init__(self):
    Driver.__init__(self, extraExact=self.EXTRA_EXACT_HANDLERS,
                    extraRegexp=self.EXTRA_REGEXP_HANDLERS)
    self.__pathArraySize = environ.get("PT_ARRAY_SIZE", "")
    self.__hashSize = environ.get("PT_HASH_SIZE", "")
    self.__silent = strtobool(environ.get("CSI_SILENT", "0").strip())
    self.__filter = True
    self.__traceFile = None
    self.__debugPass = ""
  
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
    if self.__debugPass == "all":
      yield "--debug-pass=Structure"
      yield "-debug"
    elif self.__debugPass == "prep":
      yield "-debug-only=csi-prep"
    elif len(self.__debugPass.strip()) > 0:
      yield "-debug-only=" + self.__debugPass.strip()
    
    # instrumentation plug-in
    yield "-load"
    yield os.path.join(PATH_TO_CSI_RELEASE, "libCSI.so")
    yield "-csi"
    if self.__traceFile:
      yield "-csi-variants-file"
      yield self.__traceFile
    if not self.__filter:
      yield "-csi-no-filter"
    if self.__silent:
      yield "-csi-silent"
    
    # path tracing instrumentation
    yield "-pt-inst"
    yield "-pt-info-file"
    yield self.__ptFile
    for arg in self.__checkPositiveInt(self.__pathArraySize, '-pt-path-array-size', 'path tracing array size'):
      yield arg
    for arg in self.__checkPositiveInt(self.__hashSize, '-pt-hash-size', 'path count "hash" size'):
      yield arg
    if self.__silent:
      yield "-pt-silent"
    if self.__debugPass == "pt":
      yield "-debug-only=path-tracing"
    
    # call coverage instrumentation
    yield "-call-coverage"
    yield "-cc-info-file"
    yield self.__ccFile
    if(self.__silent):
      yield "-cc-silent"
    if self.__debugPass == "cc":
      yield "-debug-only=call-coverage"
    
    # basic block coverage instrumentation
    yield "-bb-coverage"
    yield "-bbc-info-file"
    yield self.__bbcFile
    if(self.__silent):
      yield "-bbc-silent"
    if self.__debugPass == "bbc":
      yield "-debug-only=bb-coverage"
    
    # function coverage instrumentation
    yield "-fn-coverage"
    yield "-fc-info-file"
    yield self.__fcFile
    if(self.__silent):
      yield "-fc-silent"
    if self.__debugPass == "fc":
      yield "-debug-only=func-coverage"
    
  def instrumentBitcode(self, inputFile, uninstrumented, instrumented):
    # output files/directories for static/temporary data
    self.__ptFile = self.temporaryFile(inputFile, ".pt.info")
    self.__ccFile = self.temporaryFile(inputFile, ".cc.info")
    self.__bbcFile = self.temporaryFile(inputFile, ".bbc.info")
    self.__fcFile = self.temporaryFile(inputFile, ".fc.info")

    super(CSIDriver, self).instrumentBitcode(inputFile, uninstrumented, instrumented)

  def __embedSection(self, objectFile, section, filename):
    if not os.path.exists(filename): return
    if os.path.getsize(filename) == 0: return
    self.run(('objcopy', '--add-section', '.debug_%s=%s' % (section, filename), objectFile))
  
  def compileTo(self, inputFile, objectFile, args, targetFlag):
    super(CSIDriver, self).compileTo(inputFile, objectFile, args, targetFlag)
    self.__embedSection(objectFile, 'PT', self.__ptFile)
    self.__embedSection(objectFile, 'CC', self.__ccFile)
    self.__embedSection(objectFile, 'BBC', self.__bbcFile)
    self.__embedSection(objectFile, 'FC', self.__fcFile)

  def __buildCSIHelp(self, args):
    __pychecker__ = 'unusednames=args'
    print \
"""
OVERVIEW: csi-cc instrumenting LLVM compiler

USAGE: csi-cc [options] <inputs>

OPTIONS:
  --trace=<file>          Use <file> as the input file for the tracing schemes.
                          If this option is not given, the scheme is read from
                          stdin.
  -debug-pass=<arg>       Enable printing of extremely verbose debug messages
                          for the pass specified as <arg>.  This should
                          generally not be used unless debugging instrumentors.
                          <arg> can be any of the supported instrumentors or
                          'all' (which enables debugging for all passes).
  -path-array-size <arg>  Use <arg> as the size of path tracing arrays
                          (Default: 10)
  -hash-size <arg>        Use <arg> as the maximum-size function (in number of
                          acyclic paths) to instrument for path tracing
                          (Default: ULONG_MAX/2+1)
  -no-filter              Do not filter tracing schemes.  All schemes provided
                          will be used verbatim for functions to which they
                          match.
  --silent                Do not print pass-specific warnings during
                          instrumentation
  
  --help                  Display this help message and exit
  --help-clang            Display additional options (clang's help message) and
                          exit

ENVIRONMENT VARIABLES:
  PT_ARRAY_SIZE           See -path-array-size (above).  Flags have precedence.
  PT_HASH_SIZE            See -hash-size (above).  Flags have precedence.
  CSI_SILENT              Enables or disables the printing of instrumentation
                          warnings.
                          See --silent (above).  Flags have precedence.
"""

def main():
  drive(CSIDriver())

if __name__ == '__main__':
  main()
