#====----------------------------- llvm.py --------------------------------====#
#
# Utility functions to verify LLVM installation and version.
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
from SCons.Script import *
from distutils.version import StrictVersion

from sys import stderr


def check_llvm_bindir(context):
    context.Message('Checking for LLVM binaries...')
    success, output = context.TryAction('$LLVM_CONFIG --bindir >$TARGET')
    output = output.rstrip()
    context.Result(str(output))
    context.env['LLVM_bindir'] = output


def check_llvm_version(context):
    context.Message('Checking for LLVM version...')
    success, output = context.TryAction('$LLVM_CONFIG --version >$TARGET')
    # some releases of LLVM 3.2 shipped as version 3.2svn
    output = output.replace('svn', '')
    if(not output):
      context.Result("No LLVM version could be found")
      exit(1)
    output = StrictVersion(output.rstrip())
    context.Result(str(output))
    context.env['LLVM_version'] = output


def generate(env):
    if 'LLVM_version' in env:
        return

    conf = Configure(
        env,
        custom_tests={
            'CheckLLVMBinDir': check_llvm_bindir,
            'CheckLLVMVersion': check_llvm_version,
            },
        help=False,
    )
    conf.CheckLLVMVersion()
    conf.CheckLLVMBinDir()
    conf.Finish()


def exists(env):
    return true
