# -*- Mode: Python -*-

import SCons.Script

output = '#bin'

# def guess_include(name):
#     libs_inc = SCons.Script.Glob('#lib/%s/include' % name)
#     apps_inc = SCons.Script.Glob('#apps/%s/include' % name)
#     return [ i.rstr() for i in libs_inc + apps_inc ]

def guess_include(name):
    return [ ('#%s/%s/include') % (f, name) for f in ['lib', 'apps']]

def AppEnv(tenv, libs, memsize):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = guess_include(lib),
                   LIBS = [ lib ])
    env.Append(LINKFLAGS = ['--defsym=__memsize=%#x' % memsize])
    return env

def LibEnv(tenv, libs):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = guess_include(lib))
    return env

def App(tenv, name, SOURCES = [], INCLUDE = [], LIBS = [],
        LINKSCRIPT = "#vsys/linker.ld", MEMSIZE = 1<<23):
    env = LibEnv(tenv, INCLUDE)
    env = AppEnv(env,  LIBS, MEMSIZE)
    return env.Link(output + '/apps/%s.nul' % name,
                    SOURCES,
                    linkscript = LINKSCRIPT)

def Lib(tenv, name, SOURCES = [], INCLUDE = [], LIBS = []):
    env = LibEnv(tenv, INCLUDE + LIBS + [ name ])
    return env.StaticLibrary(output + "/lib/%s" % name,
                             SOURCES)

# EOF
