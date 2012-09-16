#!/usr/bin/env python2

import sys
import re
import os
import os.path
import string
import time

re_date = re.compile('^Date: (.*)')
re_testing = re.compile('^(\([0-9]+\) (#   )?)?\s*Testing "(.*)" in (.*):\s*$')
re_commit = re.compile('.*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*, commit: (.*)')
re_commithash = re.compile('([0-9a-f]{7})(-dirty)? \(')
re_check = re.compile('^(\([0-9]+\) (#   )?)?!\s*(.*?)\s+(\S+)\s*$')
re_perf =  re.compile('^(\([0-9]+\) (#   )?)?!\s*(.*?)\s+PERF:\s*(.*?)\s+(\S+)\s*$')

# State variables
date = time.localtime(time.time())
linetype = None
what = None
where = ""
commit = None
commithash = None
basename = None
ext = None
perf = None
key = None
val = None
units = None
tag = None

def matches(re):
    global match, line
    match = re.match(line)
    return match

for line in sys.stdin.readlines():
    line = line.rstrip()
    match = None

    # Parse known lines
    if matches(re_date):
        linetype='date'
        date = time.strptime(match.group(1), "%a, %d %b %Y %H:%M:%S +0200")
    elif matches(re_testing):
        linetype='testing'
        what = match.group(3)
        where = match.group(4)

        match = re_commit.match(what)
        if match:
            linetype='commitid'
            date = time.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")
            commit = match.group(2)
            match = re_commithash.search(commit);
            if match:
                commithash = match.group(1)
            else:
                commithash = None

        (basename, ext) = os.path.splitext(os.path.basename(where))
    elif matches(re_perf):
        linetype='perf'
        perf = match.group(4)
        perf = perf.split()
        key = perf[0]
        try:
            val = float(perf[1])
        except ValueError:
            val = None
        try:
            units = perf[2]
        except:
            units = None
    else:
        linetype='other'
        continue

    # Rewriting rules
    if '/vancouver-kernelbuild' in where:
        if linetype == 'testing':
            m = re.compile('vancouver-kernelbuild-(.*).wv').search(where)
            if m: tag = m.group(1)
            else: tag = 'ept-vpid'

            line='Testing "Kernel compile in ramdisk" in kernelbuild-ramdisk:'
        if linetype == 'perf':
            line = line.replace('kbuild', "vm-"+tag);
            line = line.replace('ok', 'axis="kbuild" ok');
    if '/kernelbuild-bare-metal.wv' in where:
        if linetype == 'testing':
            line='Testing "Kernel compile in ramdisk" in kernelbuild-ramdisk:'
        if linetype == 'perf':
            line = line.replace('kbuild', 'bare-metal');
            line = line.replace('ok', 'axis="kbuild" ok');

    if '/diskbench-vm.wv' in where and linetype == 'perf' and commithash == '7459b8c':
        # Skip results of test with forgotten debugging output
        continue

    if 'standalone/basicperf.c' in where and linetype == 'perf' and "PERF: warmup_" in line:
        # Skip warmup results
        continue

    if 'vancouver-linux-basic' in where:
        continue                # Ignore the old test

    if 'diskbench-ramdisk.wv' in where or 'diskbench-ramdisk-old.wv' in where:
        # Merge graphs for old and new disk protocol
        if linetype == 'testing' and 'diskbench-ramdisk-old.wv' in where:
            line = line.replace('diskbench-ramdisk-old.wv', 'diskbench-ramdisk.wv');
        if linetype == 'perf' and key == 'request_rate':
            continue # Do not plot request rate
        if linetype == 'perf' and key == 'throughput' and units:
            line = line.replace('ok', 'axis="throughput" ok');
            if 'diskbench-ramdisk-old.wv' in where:
                line = line.replace('throughput', 'old-protocol', 1)

    if 'parentperf.' in where or 'parentperfsmp.' in where:
        if linetype == 'testing':
            if 'parentperf.wv' in where: smp = False
            elif 'parentperfsmp.wv' in where: smp = True
            if what == 'Service without sessions': tag = 'nosess'
            elif what == 'Service with sessions':  tag = 'sess'
            elif what == 'Service with sessions (implemented as a subclass of SService)': tag = 'sserv'
            elif what == 'Service with sessions represented by portals (implemented as a subclass of NoXlateSService)': tag = 'noxsserv'
            else: tag = None
        elif linetype == 'perf':
            if key == 'min' or key == 'max': continue
            if key == 'open_session':
                if 'pre-vnet-removal-239-g910c152' in commit or 'pre-vnet-removal-240-g9b2fa79' in commit: continue # Broken measurements
                if not smp: print 'Testing "Parent protocol open_session performance" in parentperf_open:'
                else:       continue
            else:
                if not smp: print 'Testing "Parent protocol call performance" in parentperf_call:'
                else:       print 'Testing "Parent protocol call performance (4 CPUs in parallel)" in parentperf_call_smp:'
            line = line.replace(key, tag+'_'+key);
            print line
        continue

    if 'loc.wv' in where:
        if linetype == 'testing' and 'PASSIVE' in what: line = line.replace('loc.wv', 'loc-passive.wv');
        if linetype == 'perf' and key != 'files':
            line = line.replace('ok', 'axis="lines" ok');

    if 'pingpong.wv' in where:
        if linetype == 'perf':
            if 'min' in key or 'max' in key: continue

    if 'vancouver-boottime.wv' in where and linetype == 'perf' and key == 'tsc':
        line = "! PERF: boottime %f s ok" % (val/2.66e9)
        #print >>sys.stderr, line

    # Output (possibly modified) line
    print line

# Local Variables:
# compile-command: "cat nul-nightly/nul_*.log|./wvperfpreprocess.py|./wvperf2html.py > graphs.html"
# End:
