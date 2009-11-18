#-*- mode: python -*-

import os
def find_path(path, filename):
    "find a path in the repositories"
    repos = config.get([], "repositories")
    for r in repos:
        p,n = os.path.split(os.path.join(r, path))
        f = os.path.join(p, filename)
        if os.path.exists(f):
            return p,n
    raise Exception("could not find '%s' in repos %s"%(path, repos))

def search_files(*files):
    return map(lambda x: os.path.normpath(os.path.join(x[0], x[1])), map(lambda z: find_path(z, ""), files))

def has_changed(dest, sources):
    "check whether a dest is newer than its sources"
    # we could cache results here, but we do not have to
    try:
        return os.stat(dest).st_mtime <= reduce(max, map(lambda x: os.stat(x).st_mtime, sources))
    except OSError:
        return True


class LoadLocalHectic(Job):
    "find definitions for a given goal by loading a Hectic file from the repositories"
    def execute(self, queue):
        cfgfile = config.get("", "hectic:configfile")
        goal = self.name
        path,appname = find_path(goal, cfgfile)
        if appname:  goal = os.path.split(goal)[0]
        f = os.path.join(path, cfgfile)
        goals = config.get([], "config:"+f)
        if not goals:
            execfile(f, globals(), {"goals" : goals})
            config["config:"+f] = goals
        if appname:
            names = map(lambda x: x.name, goals)
            if not appname in names:
                raise Exception("app '%s' unknown in %s"%(appname, names))
            goals = [goals[appname]]
        for job in goals:
            job.path = path
            job.goal = goal
            job.name = os.path.join(goal, job.name)
            queue.put(job)
        debug("LoadLocal", path, appname, config.values.keys())
        
class NovaApp(Job):
    def execute(self, queue):
        debug("NovaApp", self.name)
        l = NovaLinking(self.name, env=self.env, goal=self.goal, linkfile=self.linkfile)
        objs = map(lambda x: type(x) != type("") and x or Buildgcc(os.path.join(self.goal, x), env=self.env, include=self.include, goal=self.goal), self.src)
        l.objects = map(lambda x: x.dest(), objs)
        l.add_deps(queue, objs)

class ShellJob(Job):
    def system(self, cmd):
        debug("sh", cmd)
        os.system(cmd)

class NovaLinking(ShellJob):
    def execute(self, q):
        linkfile = search_files(os.path.join(self.goal, self.linkfile))[0]
        if has_changed(self.name, self.objects+[linkfile]):
            self.system("ld -N -m elf_i386 -gc-sections -o %s -T %s %s"%(self.name, linkfile, " ".join(self.objects)))
        ShellJob.execute(self, q)


class Buildgcc(ShellJob):
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
                    " ".join(map(lambda x: "-I%s"%(x), search_files(*map(lambda x: os.path.join(self.goal, x), self.include)))),
                    " ".join(search_files(self.name))))
        ShellJob.execute(self, q)


for goal in config.get("","goals").split():  
    h.put(LoadLocalHectic(goal))
