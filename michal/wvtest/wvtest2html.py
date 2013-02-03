#!/usr/bin/env python
#
# WvTest:
#   Copyright (C) 2012 Michal Sojka <sojka@os.inf.tu-dresden.de>
#       Licensed under the GNU Library General Public License, version 2.
#       See the included file named LICENSE for license information.
#
# This script converts wvtest protocol output to HTML pages.

import sys
import re
import os
import os.path
import string
import time
import numpy as np
import cgi

re_prefix = "\([0-9]+\) (?:#   )?"
re_date = re.compile('^Date: (.*)')
re_testing = re.compile('^('+re_prefix+')?\s*Testing "(.*)" in (.*):\s*$')
re_commit = re.compile('.*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*, commit: (.*)')
re_commithash = re.compile('([0-9a-f]{7})(-dirty)? \(')
re_assertion = re.compile('^('+re_prefix+')?!\s*(.*?)\s+(\S+)\s*$')
re_perf =  re.compile('^('+re_prefix+')?!\s*(.*?)\s+PERF:\s*(.*?)\s+(\S+)\s*$')
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
        match = re_assertion.match(line)
        if match:
            self.check_count += 1
            result = match.group(3)
            if result != "ok":
                self.status = result
                self.failures += 1
    def title(self):
        if self.what == "all":
            title = self.where
        else:
            title = '%s (%s)' % (self.what, self.where)
	return title

    def printSummaryHtml(self, file):
        if self.status == "ok": status_class="ok"
        else: status_class = "failed"
        file.write("<tr class='testheader status-%s'><td class='testnum'>%d.</td><td class='testname'><a href='test%d.html'>%s</a></td>"
		   % (status_class, self.num, self.num, cgi.escape(self.title())))
        file.write("<td>%s</td></tr>\n" % (cgi.escape(self.status)))

    def printDetailHtml(self, file):
	file.write("""\
<!DOCTYPE HTML>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>NUL Test Report</title>
<link rel="stylesheet" href="wvtest.css" type="text/css" />
</head>

<body>
<h1>NUL Test Report</h1>
%s
<h2>%d. %s</h2>
<table class='output'>
""" % (date_and_commit, self.num, cgi.escape(self.title())))
        for line in self.output:
            match = re_assertion.match(line)
            if match:
                result = match.group(3)
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

            file.write("<tr><td class='outputline%s'>%s</td><td%s>%s</td></tr>\n" % \
                (linestatus, cgi.escape(line), resultstatus, cgi.escape(result)))
	file.write("</table></body></html>")

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
        what = match.group(2)
        where = match.group(3)

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

targetDir = sys.argv[1]
if not os.path.isdir(targetDir):
    os.mkdir(targetDir)

wvtest_css = open(os.path.join(targetDir, "wvtest.css"), 'w')
wvtest_css.write("""\
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
""")
wvtest_css.close()

index_html = open(os.path.join(targetDir, "index.html"), 'w')

index_html.write("""\
<!DOCTYPE HTML>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>NUL Test Report</title>
<link rel="stylesheet" href="wvtest.css" type="text/css" />
</head>

<body>
<h1>NUL Test Report</h1>
%s
<table>
""" % date_and_commit)
for test in tests_nonempty:
    test.printSummaryHtml(index_html)
index_html.write("""\
</table>
</body>
</html>
""")

for test in tests_nonempty:
    f = open(os.path.join(targetDir, "test%d.html" % test.num), 'w')
    test.printDetailHtml(f)
    f.close()


# Local Variables:
# compile-command: "cat $(ls nul-nightly/nul_*.log|tail -n 1)|./wvtest2html.py html"
# End:
