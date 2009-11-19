#!/usr/bin/env python
"""Hectic is an event based job system."""

class State:
    def __init__(self):
        self.values = {}
    def get(self, default, *names):
        "Get a value by with optional names. At least one name must be defined"
        for x in range((1 << len(names)) - 1):
            s = []
            for i in range(len(names)):
                s.append(~x & (1 << i) and names[i] or "")
            st = ":".join(s)
            if self.values.has_key(st):  return self.values[st]
        return default
    def __getitem__(self, key): return self.values[key]
    def __setitem__(self, key, value): self.values[key] = value
    def setdefault(self, key, value):  self.values.setdefault(key, value)
    def has_key(self, key):     return self.values.has_key(key)

def debug(postfix, name, *params):
    "output a debug string if a debug.postfix.name config option is present"
    if config.get(None, "debug", postfix, name):
        d = config.get(80, "debuglen")
        s = "%s\t%s %s"%(postfix, name," ".join(map(str, params)))
        if d < len(s):
            s = s[:d/2]+ " ... " + s[-d/2:]
        print "\t", s

class Job:
    "Jobs have a name and can have dependencies and return failure codes."
    def __init__(self, name, **params):
        self.name = name
        self.rdeps = []
        self.wait  = 0
        self.res   = None
        self.__dict__.update(params)
        if params.has_key("deps"):
            self.add_dep(*self.deps)
    def add_dep(self, *deps):
        "add jobs that need to be finished before we can be executed"
        self.wait += len(deps)
        for s in deps:
            s.rdeps.append(self)

    def execute(self, hectic):
        """execute this event, new events can be generated and be put into the workqueue
        If we are finished successful, we wakeup Nodes that depend on ourself."""

        if not self.res:
            debug("...", "success", self.__class__.__name__, self.name)
            self.wait -= 1
            for r in self.rdeps:
                assert r.wait,(r.wait, self)
                r.wait -= 1
                if r.wait == 0:
                    hectic.put(r)
        else:
            debug("...", "error", self.res, self.__class__.__name__, self.name)

class Hectic:
    """Hectic consists of a couple worker threads waiting on a queue for
    jobs to execute."""
    def __init__(self):
        import Queue
        self.q = Queue.Queue()
    def worker(self):
        while True:
            try:
                job = self.q.get()
                debug("---", job.name)
                job.execute(self)
            except:
                import traceback
                traceback.print_exc()
            debug("<--", job.name)
            self.q.task_done()
    def put(self, job):
        "put something in the work queue"
        debug("-->", job.name)
        self.q.put(job)
    def run(self):
        "run and wait for all jobs to finish"
        import threading
        for i in range(config.get(1, "hectic:threads")):
            t = threading.Thread(target=self.worker)
            t.setDaemon(True)
            t.start()
        self.q.join()


config = State()
if __name__ == "__main__":
    import sys, os

    # default parameters
    config["hectic:threads"] = 8
    config["hectic:configfile"] = "Hectic"
    h = Hectic()

    # evaluate cmdline params
    for arg in sys.argv[1:]:
        key,value = arg, None
        try:
            key, value = arg.split("=")
            value = int(value)
        except ValueError:
            pass
        config[key] = value

    # config file
    exec open(config.get("", "hectic:configfile"))

    # run the jobs
    h.run()
