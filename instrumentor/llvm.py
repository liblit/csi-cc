#====----------------------------- llvm.py --------------------------------====#
#
# Utility functions to verify LLVM installation and version.
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
from SCons.Script import *
from distutils.version import StrictVersion

from sys import stderr

import os.path


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

def check_llvm_assertions(context):
    context.Message('Checking for LLVM assertions...')
    success, output = context.TryAction('$LLVM_CONFIG --assertion-mode >$TARGET')
    if not success:
        # old versions of LLVM don't have --assertion-mode, so optimistically
        # assume that we have assertions enabled
        output = 'on'
    output = output.rstrip()
    context.Result(output)
    if output.strip().lower() == 'on':
        context.env['LLVM_assertions'] = True

def __libExists(libDir, libName, env):
    fullLibName = env.subst('$SHLIBPREFIX') + libName + env.subst('$SHLIBSUFFIX')
    return os.path.isfile(os.path.join(libDir, fullLibName))

def check_llvm_sharedlib(context):
    # TODO: use $LLVM_CONFIG --link-shared --libs for LLVM versions 3.9+
    # This changes the procedure a bit, since it actually returns the entire
    # flag to be passed to the linker.

    context.Message('Checking for LLVM shared library name...')
    success, output = context.TryAction('$LLVM_CONFIG --libdir >$TARGET')
    if not success or not output:
        context.Result('No LLVM libraries found')
        Exit(1)
    output = output.rstrip()

    libName = context.env.subst('LLVM-$LLVM_version${("", "svn")[LLVM_version == "3.2"]}')
    if not __libExists(output, libName, context.env):
        libName = 'LLVM'
        if not __libExists(output, libName, context.env):
            context.Result('LLVM not built with --enable-shared')
            Exit(1)
    context.Result(libName)
    context.env['LLVM_libname'] = libName


def generate(env):
    if 'LLVM_version' in env:
        return

    conf = Configure(
        env,
        custom_tests={
            'CheckLLVMAssertions': check_llvm_assertions,
            'CheckLLVMBinDir': check_llvm_bindir,
            'CheckLLVMSharedLib': check_llvm_sharedlib,
            'CheckLLVMVersion': check_llvm_version,
            },
        clean=False,
        config_h='instrumentor/config.h',
        help=False,
    )
    # order matters: version and bindir must come first
    conf.CheckLLVMVersion()
    conf.CheckLLVMBinDir()
    conf.CheckLLVMAssertions()
    conf.CheckLLVMSharedLib()

    # very strange bug requires that we check env for LLVM_bindir here.  If this
    # check is removed, scons 2.0.1 will try to run the "WhereIs" before the
    # above conf checks
    if 'LLVM_bindir' in env:
        if not env.WhereIs('clang', env['LLVM_bindir']):
            raise SCons.Errors.UserError("Cannot find 'clang' in '%s'" % env['LLVM_bindir'])

    if 'LLVM_version' in env:
        versionParts = map(int, str(env['LLVM_version']).split('.'))
        versionValue = versionParts[0] * 10000 + versionParts[1] * 100
        if len(versionParts) > 2:
            versionValue += versionParts[2]
        conf.Define('LLVM_CONFIG_VERSION', versionValue)

    conf.Finish()


def exists(env):
    return true
