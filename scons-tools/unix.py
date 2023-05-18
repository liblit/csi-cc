#====----------------------------- unix.py --------------------------------====#
#
# Utility functions to get various configuration details from Unix utilities.
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

import os.path
import platform


def check_obj(context, toolName):
    fullToolName = 'obj%s' % toolName
    context.Message('Checking for %s...' % fullToolName)

    toolsToTry = [fullToolName, 'g%s' % fullToolName]
    if 'darwin' in platform.system().lower():
        # on OSX, prefer gobjcopy
        toolsToTry.reverse()

    objPath = context.env.WhereIs(toolsToTry[0])
    if not objPath:
        objPath = context.env.WhereIs(toolsToTry[1])
    if not objPath:
        context.Result("No %s or %s found" % tuple(toolsToTry))
        Exit(1)
    context.Result(objPath)
    context.env['%sEXE' % fullToolName.upper()] = objPath


def generate(env):
    if 'OBJCOPYEXE' in env:
        return

    conf = Configure(
        env,
        custom_tests={
            'CheckObj': check_obj,
            },
        clean=False,
        help=False,
    )
    conf.CheckObj('copy')
    conf.CheckObj('dump')

    conf.Finish()


def exists(env):
    return true
