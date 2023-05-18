########################################################################
#
#  configurable options
#

from os import access, environ, X_OK

def pathIsExecutable(key, val, env):
    found = env.WhereIs(val)
    if found: val = found
    PathVariable.PathIsFile(key, val, env)
    if not access(val, X_OK):
        raise SCons.Errors.UserError('Path for option %s is not executable: %s' % (key, val))

    
opts = Variables('.scons-config', ARGUMENTS)
opts.AddVariables(
    PathVariable(key='LLVM_CONFIG',
                 help='path to llvm-config executable for LLVM installation',
                 default=WhereIs('llvm-config'),
                 validator=pathIsExecutable),
    PathVariable(key='CXX_ALT',
                 help='path to alternate C++ compiler',
                 default=None,
                 validator=pathIsExecutable),
    PathVariable(key='GAMSDIR',
                 help='path to GAMS installation directory',
                 default=None,
                 validator=PathVariable.PathIsDir),
    PathVariable(key='LEMONDIR',
                 help='path to LEMON installation directory.  Requires GUROBIDIR',
                 default=None,
                 validator=PathVariable.PathIsDir),
    PathVariable(key='GUROBIDIR',
                 help='path to GUROBI installation directory',
                 default=None,
                 validator=PathVariable.PathIsDir),
    BoolVariable(key='DEBUG',
                 help='Compile with extra information for debugging',
                 default=False),
    BoolVariable(key='MEMCHECK',
                 help="Run instrumentor tests under Valgrind's memcheck tool",
                 default=False),
)
env = Environment(
    options=opts,
    tools=('default', 'llvm', 'textfile', 'unix'),
    toolpath=('instrumentor', 'scons-tools',),
)
opts.Save('.scons-config', env)

if env.get('LEMONDIR', None) and not env.get('GUROBIDIR', None):
    print 'Must specify both LEMONDIR and GUROBIDIR'
    Exit(1)

########################################################################
#
# performance boosters
#

CacheDir('.scons-cache')
#SetOption('implicit_cache', True)
SetOption('max_drift', 1)


########################################################################
#
#  shared build environment
#

env = env.Clone(
    CCFLAGS=(
        '-Wall',
        '-Wextra',
        '-Werror',
        '-Wformat=2',
        '-Wno-deprecated-declarations',
        '${("", "-g")[DEBUG]}',
    ),
)

Help(opts.GenerateHelpText(env))
Export('env')


########################################################################
#
#  subsidiary scons scripts
#

excludedSources = set(['config.log'])
Export('excludedSources')

SConscript(dirs=[
    'instrumentor',
    'driver',
    'tests',
    'Tools',
])

Default('Release')
Depends('Release', 'Tools')
Depends('test', 'Tools')


########################################################################
#
#  packaging
#

version=File('version').get_contents().rstrip()

env.File([
    'AUTHORS',
    'COPYING',
    'NEWS',
    'README',
    'UI_LICENSE.txt',
])

excludedSources = set(map(env.File, excludedSources))
sources = set(env.FindSourceFiles()) - excludedSources
sources = sorted(sources, key=lambda node: node.path)

env.Tool('packaging')
package = env.Package(
    NAME='csi',
    VERSION=version,
    PACKAGETYPE='src_targz',
    source=sources,
)[0]

AddPostAction(package, Chmod('$TARGET', 0644))
