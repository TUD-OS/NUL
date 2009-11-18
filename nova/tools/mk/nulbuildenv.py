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
    raise Exception("could not find '%s' in repos %s"%(os.path.join(path, filename), repos))

def search_files(*files):
    "search files in the repositories"
    return map(lambda x: os.path.normpath(os.path.join(x[0], x[1])), map(lambda z: find_path(z, ""), files))

def has_changed(dest, *sources):
    "check whether a dest is newer than its sources"
    # we could cache results here, but we do not have to
    try:
        return os.stat(dest).st_mtime <= reduce(max, map(lambda x: os.stat(x).st_mtime, sources))
    except OSError:
        return True


class LoadLocalHectic(Job):
    "find definitions for a given goal by loading a Hectic file from the repositories"
    def execute(self, hectic):
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
            job.add_dep(self)
        debug("Load", path, appname, config.values.keys())
        Job.execute(self, hectic)
        
class ShellJob(Job):
    "A job that executes a shell command"
    def system(self, cmd):
        debug("sh", cmd)
        self.res = self.res or os.system(cmd)

class NulObject(ShellJob):
    "build an object file from a source"
    def dest(self):
        return self.name+".o"
    def changed(self):
        if os.path.exists(self.name+".d"):
            l = open(self.name+".d").read().replace("\\\n", "").split(":")
            assert len(l) == 2 and l[0] == self.dest()
            return has_changed(self.dest(), *l[1].split())
        return True
    def execute(self, hectic):
        env = self.env
        if self.changed():
            is_cpp = self.name.endswith(".cc")
            self.system("%s -c -MD -g %s %s %s -o %s"%(
                    is_cpp and "g++" or "gcc",
                    env[is_cpp and "CXXFLAGS" or "CFLAGS"],
                    " ".join(map(lambda x: "-I%s"%(x), search_files(*map(lambda x: os.path.join(self.goal, x), self.include)))),
                    " ".join(search_files(self.name)), self.dest()))
        ShellJob.execute(self, hectic)


class NulLink(ShellJob):
    "link objects to an application self.name <- [self.objects]"
    def execute(self, hectic):
        linkfile = search_files(os.path.join(self.goal, self.linkfile))[0]
        if has_changed(self.name, linkfile, *self.objects):
            self.system("ld %s -o %s -T %s %s"%(self.env["LDFLAGS"], self.name, linkfile, " ".join(self.objects)))
        ShellJob.execute(self, hectic)

class StripJob(ShellJob):
    "strip an application: self.name <- self.src"
    def execute(self, hectic):
        if has_changed(self.name, self.src):
            self.system("strip -o %s %s"%(self.name, self.src))
        ShellJob.execute(self, hectic)

class GzipJob(ShellJob):
    "gzip a file: self.name <- self.src"
    def execute(self, hectic):
        if has_changed(self.name, self.src):
            self.system("gzip -c %s > %s"%(self.src, self.name))
        ShellJob.execute(self, hectic)
    

class NulApp(Job):
    "build a NUL application"
    def execute(self, hectic):
        debug("NulApp", self.name)
        objs = map(lambda x: type(x) != type("") and x or NulObject(os.path.join(self.goal, x), env=self.env, include=self.include, goal=self.goal, deps=[self]), self.src)
        link = NulLink(self.name+".nuldebug", env=self.env, goal=self.goal, linkfile=self.linkfile, objects = map(lambda x: x.dest(), objs), deps = objs)
        strip = StripJob(self.name+".nul", src=link.name, deps = [link])
        c = GzipJob(strip.name+".gz", src=strip.name, deps = [strip])
        Job.execute(self, hectic)

class CleanAll(ShellJob):
    "clean the working directory"
    def execute(self, hectic):
        self.system("find -mindepth 2 -type f -exec rm {} \\; ")
        ShellJob.execute(self, hectic)


class NulBuild(Job):
    "build everything"
    def execute(self, hectic):
        pregoals = [self]
        if config.has_key("-c"):
            pregoals = [CleanAll("cleanall", deps=pregoals)]
        for goal in config.get("","goals").split():  
            LoadLocalHectic(goal, deps=pregoals)
        Job.execute(self, hectic)

if __name__ == "__main__":
    h.put(NulBuild("build"))
