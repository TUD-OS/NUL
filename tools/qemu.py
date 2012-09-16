#!/usr/bin/env python2
"""Start qemu with kernel from pulsar style config file."""

import os, sys, re
def read_config(maindir, configfile, root="", res=[None]):
    "Read pulsar style config files and add them to a list"
    st = open(os.path.join(maindir + os.path.sep + root, configfile)).read()
    for line in st.replace("\\\n", "").split("\n"):
        line = line.split("#")[0].strip()
        if not line:  continue
        param = line[4:].strip()
        if line.startswith("root"):
            root = param
        elif line.startswith("exec"):
            res[0] = os.path.join(maindir + os.path.sep + root, param)
        elif line.startswith("load"):
            res.append(os.path.join(maindir + os.path.sep + root, param))
        elif line.startswith("conf"):
            read_config(maindir, param, root, res)
        else:
            print "ignored line:", repr(line)
    return res

if __name__ == "__main__":
    if len(sys.argv) < 3:
       print "Usage: %s configfile qemubinary [options]"%sys.argv[0]
       sys.exit(1)
    l = read_config(*os.path.split(sys.argv[1]))
    kernel = l[0].split()[0]
    args = [sys.argv[2], "-kernel", kernel, "-append", l[0][len(kernel)+1:]]
    initrd = map(lambda x: re.sub("\s+", " ", x.replace(",", "+")), l[1:])
    if initrd:
       args.extend(["-initrd", ",".join(initrd)])
    args.extend(sys.argv[3:])
    os.execv(args[0], args)
