#!/usr/bin/env python

import sys
import re
import os
import os.path
import string
import time
import numpy as np
import cgi

re_date = re.compile('^Date: (.*)')
re_testing = re.compile('^(\([0-9]+\) (#   )?)?\s*Testing "(.*)" in (.*):\s*$')
re_commit = re.compile('.*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*, commit: (.*)')
re_commithash = re.compile('([0-9a-f]{7})(-dirty)? \(')
re_check = re.compile('^(\([0-9]+\) (#   )?)?!\s*(.*?)\s+(\S+)\s*$')
re_perf =  re.compile('^(\([0-9]+\) (#   )?)?!\s*(.*?)\s+PERF:\s*(.*?)\s+(\S+)\s*$')
re_perfaxis = re.compile('axis="([^"]+)"')

date = time.localtime(time.time())

class Test:
    def __init__(self, what, where):
        self.what = what
        self.where = where
        self.output = []
        self.status = 'ok'
        self.check_count = 0
        self.failures = 0
        self.num = None

    def add_line(self, line):
        self.output.append(line)
        match = re_check.match(line)
        if match:
            self.check_count += 1
            result = match.group(4)
            if result != "ok":
                self.status = result
                self.failures += 1

    def printHtml(self):
        if self.what == "all":
            title = self.where
        else:
            title = '%s (%s)' % (self.what, self.where)
        if self.status == "ok": status_class="ok"
        else: status_class = "failed"
        print "<tr class='testheader status-%s'><td class='testnum'>%d.</td><td class='testname'><a href='#test%d'>%s</a></td>" % (status_class, self.num, self.num, cgi.escape(title)),
        print "<td>%s</td></tr>" % (cgi.escape(self.status))
        print "<tr class='outputrow' id='test%d'><td></td><td colspan='2'><table class='output'>" % self.num
        for line in self.output:
            match = re_check.match(line)
            if match:
                result = match.group(4)
                if result == "ok":
                    status_class = "ok"
                else:
                    status_class = "failed"
                linestatus = " status-%s" % status_class
                resultstatus = " class='status-%s'" % status_class
            else:
                linestatus = ''
                resultstatus = ''
                result = ''

            print "<tr><td></td><td class='outputline%s'>%s</td><td%s>%s</td></tr>" % \
                (linestatus, cgi.escape(line), resultstatus, cgi.escape(result))
        print "</table></td></tr>"

tests = []
test = None

for line in sys.stdin.readlines():
    line = line.rstrip()

    match = re_date.match(line)
    if (match):
        date = time.strptime(match.group(1), "%a, %d %b %Y %H:%M:%S +0200")
        continue

    match = re_testing.match(line)
    if match:
        what = match.group(3)
        where = match.group(4)

        test = Test(what, where)
        tests.append(test)

        match = re_commit.match(what)
        if match:
            date = time.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")
            commit = match.group(2)
            match = re_commithash.search(commit);
            if match:
                commithash = match.group(1)
            else:
                commithash = None
        continue

    if test: test.add_line(line)

tests_nonempty = [t for t in tests if t.check_count > 0]
num = 1
for t in tests_nonempty:
    t.num = num
    num += 1

try:
    date_and_commit = (time.strftime("%a, %d %b %Y %H:%M:%S +0000", date) + " " + commit)
except:
    date_and_commit = time.strftime("%a, %d %b %Y %H:%M:%S %Z")
    pass
print """<!DOCTYPE HTML>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>NUL Test Report</title>
<style>
table {
  border: solid 1px black;
  max-width: 100%%;
}
.status-ok { background: lightgreen; }
.status-failed { background: red; }
.testnum { text-align: right; }
.outputrow { display: none; }
.output { width: 100%%; }
.outputline { white-space: pre-wrap; font-family: monospace; }
.testheader { font-weight: bold; }
</style>
<script src="http://code.jquery.com/jquery-latest.js"></script>
<script>
$(document).ready(function(){
   $(".testheader a").click(function(event){
     $($(this).attr('href')).toggle();
     //$("#ipctest-wv").toggle();
     event.preventDefault();
   });
 });
</script>
</head>

<body>
<h1>NUL Test Report</h1>
%s
<table>
""" % date_and_commit
for test in tests_nonempty:
    test.printHtml()
print """
</table>
</body>
</html>
"""

# Local Variables:
# compile-command: "cat $(ls nul-nightly/nul_*.log|tail -n 1)|./wvtest2html.py > test-report.html"
# End:
