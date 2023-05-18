#====----------------------------- expect.py ------------------------------====#
#
# This script defines scons builders to compare output files.
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

import filecmp

from expectcmp import read_rt, read_pt, RTType

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

def __compare_action_cc(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return sorted(read_rt(str(actual), RTType.CC)) != sorted(read_rt(str(expected), RTType.CC))

def __compare_action_bbc(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return sorted(read_rt(str(actual), RTType.BBC)) != sorted(read_rt(str(expected), RTType.BBC))

def __compare_action_pt(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    actual_pt = read_pt(str(actual))
    expected_pt = read_pt(str(expected))
    return any(not x.verify() for x in actual_pt) or \
           any(not x.verify() for x in expected_pt) or \
           sorted(actual_pt) != sorted(expected_pt)

def __compare_action_show(target, source, env):
    __pychecker__ = 'no-argsused'
    [actual, expected] = source
    return 'compare "%s" and "%s"' % (actual, expected)

__specialized_compare_actions = {
    '.csi-static-BBC': __compare_action_bbc,
    '.csi-static-FC': __compare_action_lines,
    '.csi-static-CC': __compare_action_cc,
    '.csi-static-CT': __compare_action_cc,
    '.csi-static-PT': __compare_action_pt,
}

def __compare_action(target, source, env):
    extension = SCons.Util.splitext(source[0].name)[1]
    action = __specialized_compare_actions[extension]
    return action(target, source, env)

def __action_emitter(target, source, env):
    [actual] = source
    expected = env.File(actual.name+'.expected')
    return env.File(actual.name+'.passed'), [actual, expected]


__static_builder = Builder(
    emitter=__action_emitter,
    action=[Action(__compare_action, __compare_action_show), Touch('$TARGET')],
)


__exact_builder = Builder(
    emitter=__action_emitter,
    action=[Action(__compare_action_exact, __compare_action_show), Touch('$TARGET')],
)


########################################################################


def generate(env):
    env.AppendUnique(
        BUILDERS={
            'ExpectStaticCSI': __static_builder,
            'ExpectExact': __exact_builder,
        },
    )


def exists(env):
    return True
