########################################################################
#
#  configurable options
#

opts = Variables('.scons-config', ARGUMENTS)
opts.AddVariables(
    PathVariable(key='LLVMBIN',
                 help='location of bin directory of LLVM installation',
                 default=None,
                 validator=PathVariable.PathIsDir),
    PathVariable(key='CXX_ALT',
                 help='path to alternate C++ compiler',
                 default=None,
                 validator=PathVariable.PathIsFile),
    )
env = Environment(options=opts)
opts.Save('.scons-config', env)

########################################################################
#
# performance boosters
#

CacheDir('.scons-cache')
SetOption('implicit_cache', True)
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
])


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

AddPostAction(package, [Chmod('$TARGET', 0644)])
