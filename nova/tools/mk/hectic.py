#!/usr/bin/env python

DEFFILE = "target.py"
CFGFILE = "config.py"
THREADS = 8


import  os, sys, pprint, threading, Queue, traceback

class Node:
    def __init__(self, name, params = {}, env=None):
        self.name =  name
        self._params_ = params.copy()
        self.env = env or _env_
        self["_rdeps_"] = []
        self["_done_"] = 0
        _deps_.append(self)
    def __getitem__(self, x):
        return self._params_[x]
    def __setitem__(self, x, y):
        self._params_[x] = y
    def setdefault(self, x, y):
        self._params_.setdefault(x,y)
    def __repr__(self):
        return '<%s("%s" %d)>'%(self.__class__.__name__, self.name, self["_done_"])
    def add_deps(self, q, deps):
        self["_done_"] += len(deps)
        for s in deps:
            s["_rdeps_"] += [self]
            if not s["_done_"] and q:
                q.put(s)
    def execute(self, q):
        "wakeup Nodes depending on ourself"
        self.debug_print("execute", "executed", self, self["_rdeps_"])
        for r in self["_rdeps_"]:
            assert r["_done_"],(r["_done_"], self)
            r["_done_"] -= 1
            if r["_done_"] == 0:
                q.put(r)
    def system(self,x):
        self.debug_print("system", "BUILD ", self.name)
        self.debug_print("cmd",    ">>",     x[:100])
        os.system(x)
    def debug(self, postfix=""):
        for s in ["%s.%s"%(self.name, postfix), self.name, ".%s"%postfix, "%s.%s"%(self.__class__.__name__, postfix), self.__class__.__name__]:
            if DBG.has_key(s):  return DBG[s]
    def debug_print(self, postfix, *params):
        if self.debug(postfix): print " ".join(map(str,params))

class Buildgcc(Node):
    def dest(self):
        return self.name+".o"
    def changed(self):
        if os.path.exists(self.name+".d"):
            l = open(self.name+".d").read().replace("\\\n", "").split(":")
            assert len(l) == 2 and l[0] == self.dest()
            return has_changed(self.dest(), l[1].split())
        return True
        
    def execute(self, q):
        env = self.env
        if self.changed():
            is_cpp = self.name.endswith(".cc")
            self.system("%s -c -o %s %s %s %s"%(
                    is_cpp and "g++" or "gcc",
                    self.dest(),
                    env[is_cpp and "CXXFLAGS" or "CFLAGS"],
                    " ".join(map(lambda x: "-I%s"%(x), search_files(map(lambda x: os.path.join(env["_goal_"], x), env["include"]), env["_repos_"]))),
                    " ".join(search_files([self.name], env["_repos_"]))))
        Node.execute(self, q)
class Link(Node):
    def execute(self, q):
        dest = os.path.join(self.env["_goal_"], self.name)
        linkfile = search_files([os.path.join(self.env["_goal_"], self.env["linkfile"])], self.env["_repos_"])[0]
        if has_changed(dest, self.objects+[linkfile]):
            self.system("ld -N -m elf_i386 -gc-sections -o %s -T %s %s"%(dest, linkfile, " ".join(self.objects)))
        Node.execute(self, q)
class App(Node):
    def execute(self, q):
        l = Link(self.name, env=self.env)
        objs = map(lambda x: Buildgcc(x, env=self.env), self["src"])
        l.objects = map(lambda x: x.dest(), objs)
        l.add_deps(q, objs)
class Lib(Node):
    pass


def has_changed(dest, sources):
    "check whether a dest is newer than its sources"
    # we could cache results here, but we do not have to
    try:
        return os.stat(dest).st_mtime <= reduce(max, map(lambda x: os.stat(x).st_mtime, sources))
    except OSError:
        return True

def find_path(path, repos, filename):
    "find a path in the repositories"
    for r in repos:
        p,n = os.path.split(os.path.join(r, path))
        f = os.path.join(p, filename)
        if os.path.exists(f):
            return p,n
    raise Exception("could not find '%s' in repos %s"%(path, repos))

def add_path(l, path):
    "add a path to a list of dir local entries"
    return map(lambda x: os.path.split(x)[0] and x or os.path.join(path, x), l)

def search_files(files, repos):
    return map(lambda x: os.path.normpath(os.path.join(x[0], x[1])), map(lambda z: find_path(z, repos, ""), files))

def find_definition(repos, goal, paths):
    "find all definitions for a given goal"
    res = {}
    path,appname = find_path(goal, repos, DEFFILE)
    if appname:  goal = os.path.split(goal)[0]
    env = {"_goal_": goal, "_path_": path, "_repos_" : repos, "include" : []}
    env["CXXFLAGS"] = ""
    # load the goal definitions
    if path not in paths:
        deps = []
        x = globals()
        x["_deps_"] = deps
        x["_env_"] =  env
        execfile(os.path.join(path, DEFFILE), x, {})
        deps = dict(zip(map(lambda x: x.name, deps), deps))
        paths[path] = deps
    deps = paths[path]
    if appname:
        if not appname in deps: 
            raise Exception("app '%s' unknown in %s"%(appname, app.keys()))
        deps = {appname: deps[appname]}
    print goal, appname, deps.keys()
    for name in deps:
        for p in ["deps", "src", "include"]:
            deps[name].setdefault(p, [])
            deps[name][p]    = add_path(deps[name][p], goal)
        res[os.path.join(goal, name)] = deps[name]
    return res


def make_dep(goals):
    "make dependencies for all the goals"
    repositories = []
    
    # load the config file
    if os.path.exists(CFGFILE):  exec open(CFGFILE)
    if not goals:
        raise Exception("please specify some goals on the cmdline or in %s"%CFGFILE)
    nodes = {}
    paths = {}

    # find all nodes for our goals
    for goal in goals:  nodes.update(find_definition(repositories, goal, paths))

    # resolve all deps recursively
    need_deps = nodes.copy()
    deps_done = {}
    while need_deps:
        name, n = need_deps.popitem()
        deps_done[name] = None
        for i in range(len(n["deps"])):
            lib = n["deps"][i]
            if lib not in deps_done and lib not in need_deps:
                if not lib in nodes: 
                    nodes.update(find_definition(repositories, lib, paths))
                    if not lib in nodes: raise Exception("could not find lib '%s'"%lib)
                need_deps[lib] = nodes[lib]
            n["deps"][i] = nodes[lib]
        n.add_deps(None, n["deps"])
    return nodes

def worker(q, nodes):
    while True:
        try:
            q.get().execute(q)
        except:
            traceback.print_exc()
        q.task_done()

def execute(nodes):
    "execute all nodes"
    q = Queue.Queue()
    for i in range(THREADS):
        t = threading.Thread(target=worker, args=[q, nodes])
        t.setDaemon(True)
        t.start()

    # issue leaf requests
    for n in nodes.values():
        if not n["_done_"]:
            q.put(n)
    q.join()


DBG = {"Link": 1}
DEBUG   = 1
if __name__ == "__main__":
    nodes = make_dep(sys.argv[1:])
    if DEBUG:  pprint.pprint(nodes)
    execute(nodes)



