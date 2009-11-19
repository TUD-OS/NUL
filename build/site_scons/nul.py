# -*- Mode: Python -*-

import SCons.Script

output = '#bin'

def AppEnv(tenv, libs):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = [ '#lib/%s/include' % lib ],
                   LIBS = [ lib ])
    return env

def LibEnv(tenv, libs):
    """Clone tenv and modify it to make the given libs available."""
    env = tenv.Clone()
    for lib in libs:
        env.Append(CPPPATH = [ '#lib/%s/include' % lib ])
    return env

def App(tenv, name, SOURCES = [], LIBS = ['nova'],
        LINKSCRIPT = None):
    env = AppEnv(tenv, LIBS)
    if not LINKSCRIPT:
        LINKSCRIPT = "%s.ld" % name
    return env.Link(output + '/apps/%s.nova' % name,
                    SOURCES,
                    linkscript = LINKSCRIPT)

def Lib(tenv, name, SOURCES = [], LIBS = ['nova']):
    env = LibEnv(tenv, LIBS + [ name ])
    return env.StaticLibrary(output + "/lib/%s" % name,
                             SOURCES)

# EOF
