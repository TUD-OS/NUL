# -*- Mode: Python -*-

import SCons.Script

output = '#bin'

# def guess_include(name):
#     libs_inc = SCons.Script.Glob('#lib/%s/include' % name)
#     apps_inc = SCons.Script.Glob('#apps/%s/include' % name)
#     return [ i.rstr() for i in libs_inc + apps_inc ]

def guess_include(name):
    return [ ('#%s/%s/include') % (f, name) for f in ['lib', 'apps']]

def AppEnv(tenv, libs):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = guess_include(lib),
                   LIBS = [ lib ])
    return env

def LibEnv(tenv, libs):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = guess_include(lib))
    return env

def App(tenv, name, SOURCES = [], INCLUDE = [], LIBS = ['nova'],
        LINKSCRIPT = None):
    env = LibEnv(tenv, INCLUDE)
    env = AppEnv(env,  LIBS)
    if not LINKSCRIPT:
        LINKSCRIPT = "%s.ld" % name
    return env.Link(output + '/apps/%s.nul' % name,
                    SOURCES,
                    linkscript = LINKSCRIPT)

def Lib(tenv, name, SOURCES = [], INCLUDE = [], LIBS = ['nova']):
    env = LibEnv(tenv, INCLUDE + LIBS + [ name ])
    return env.StaticLibrary(output + "/lib/%s" % name,
                             SOURCES)

# EOF
