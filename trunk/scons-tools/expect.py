#====----------------------------- expect.py ------------------------------====#
#
# This script defines scons builders to compare output files.
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

import filecmp

from expectcmp import read_rt

########################################################################
#
#  comparison with reference output
#


def __compare_action_exact(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return not filecmp.cmp(str(actual), str(expected), False)

def __compare_action_lines(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    with open(str(actual), 'r') as f1:
      with open(str(expected), 'r') as f2:
        return sorted(f1.readlines()) != sorted(f2.readlines())
    return True

def __compare_action_rt(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return sorted(read_rt(str(actual))) != sorted(read_rt(str(expected)))


def __compare_action_show(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return 'compare "%s" and "%s"' % (actual, expected)


__compare_exact = Action(__compare_action_exact, __compare_action_show)
__compare_lines = Action(__compare_action_lines, __compare_action_show)
__compare_rt = Action(__compare_action_rt, __compare_action_show)


__exact_action = [__compare_exact, Touch('$TARGET')]
__lines_action = [__compare_lines, Touch('$TARGET')]
__rt_action = [__compare_rt, Touch('$TARGET')]


def __action_emitter(target, source, env):
    [actual] = source
    expected = env.File(actual.name+'.expected')
    return env.File(actual.name+'.passed'), [actual, expected]


__exact_builder = Builder(
    action=__exact_action,
    emitter=__action_emitter,
    )

__lines_builder = Builder(
    action=__lines_action,
    emitter=__action_emitter,
    )

__rt_builder = Builder(
    action=__rt_action,
    emitter=__action_emitter,
    )


########################################################################


def generate(env):
    env.AppendUnique(
        BUILDERS={
            'ExpectExact': __exact_builder,
            'ExpectLines': __lines_builder,
            'ExpectRT': __rt_builder,
            },
        )


def exists(env):
    return True
