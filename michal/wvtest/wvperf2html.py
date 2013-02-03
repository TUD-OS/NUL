#!/usr/bin/env python
#
# WvTest:
#   Copyright (C) 2012 Michal Sojka <sojka@os.inf.tu-dresden.de>
#       Licensed under the GNU Library General Public License, version 2.
#       See the included file named LICENSE for license information.
#
# This script converts a sequence of wvtest protocol outputs with
# results of performance measurements to interactive graphs (HTML +
# JavaScript). An example can be seen at
# http://os.inf.tu-dresden.de/~sojka/nul/performance.html.

import sys
import re
import os
import os.path
import string
import time
import numpy as np

re_prefix = "\([0-9]+\) (?:#   )?"
re_date = re.compile('^Date: (.*)')
re_testing = re.compile('^('+re_prefix+')?\s*Testing "(.*)" in (.*):\s*$')
re_commit = re.compile('.*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*, commit: (.*)')
re_commithash = re.compile('([0-9a-f]{7})(-dirty)? \(')
re_assertion = re.compile('^('+re_prefix+')?!\s*(.*?)\s+(\S+)\s*$')
re_perf =  re.compile('^('+re_prefix+')?!\s*(.*?)\s+PERF:\s*(.*?)\s+(\S+)\s*$')
re_perfaxis = re.compile('axis="([^"]+)"')

class Axis:
    def __init__(self, name=None, units=None):
        self.name = name
        self.units = units
        self.num = None
    def getLabel(self):
        if self.units and self.name:
            return "%s [%s]" % (self.name, self.units)
        elif self.units:
            return self.units
        else:
            return self.name
    def __repr__(self): return "Axis(name=%s units=%s, id=%s)" % (self.name, self.units, hex(id(self)))

class Column:
    def __init__(self, name, units, axis):
        self.name = name
        self.units = units
        self.axis = axis
    def __repr__(self): return "Column(name=%s units=%s axis=%s)" % (self.name, self.units, repr(self.axis))

class Row(dict):
    def __init__(self, graph, date):
        self.graph = graph
        self.date = date
    def __getitem__(self, column):
        try:
            return dict.__getitem__(self, column)
        except KeyError:
            return None
    def getDate(self):
        d = time.gmtime(time.mktime(self.date))
        return "Date.UTC(%s, %s, %s, %s, %s, %s)" % \
            (d.tm_year, d.tm_mon-1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec)

class Graph:
    def __init__(self, id, title):
        self.columns = {}
        self.columns_ordered = []
        self.id = id
        self.title = title
        self.rows = []
        self.date2row = {}
        self.axes = {}
        self.axes_ordered = []

    def __getitem__(self, date):
        try:
            rownum = self.date2row[date]
        except KeyError:
            rownum = len(self.rows)
            self.date2row[date] = rownum
        try:
            return self.rows[rownum]
        except IndexError:
            self.rows[rownum:rownum] = [Row(self, date)]
            return self.rows[rownum]

    def addValue(self, date, col, val, units):
        row = self[date]
        row[col] = val
        if not self.columns.has_key(col):
            axis=self.getAxis(units) or self.addAxis(units, Axis(units=units))
            column = Column(col, units, axis)
            self.columns[col] = column
            self.columns_ordered.append(column)
        else:
            column = self.columns[col]
            self.columns_ordered.remove(column)
            self.columns_ordered.append(column)
        self.columns[col].units=units
        self.columns[col].axis.units=units

    def addAxis(self, key, axis):
        self.axes[key] = axis
        return axis

    def setAxis(self, col, key):
        self.columns[col].axis = self.getAxis(key) or self.addAxis(key, Axis(name=key))

    def getAxis(self, key):
        if not self.axes.has_key(key): return None
        return self.axes[key]

    def findRanges(self):
        for axis in self.axes.values():
            cols = [col for col in self.columns.values() if col.axis == axis]
            low = None
            high = None
            all_in_range = True
            for col in cols:
                values = np.array([row[col.name] for row in self.rows if row[col.name] != None], np.float64)
                if low == None and high == None:
                    lastmonth = values[-30:]
                    median = np.median(lastmonth)
                    low  = median * 0.95
                    high = median * 1.05

                if (values > high).any() or (values < low).any():
                    all_in_range = False
            if all_in_range:
                axis.yrange_max = high
                axis.yrange_min = low
            else:
                axis.yrange_max = None
                axis.yrange_min = None

    def fixupAxisNumbers(self):
        # Sort axes according to the columns and number them
        num = 0
        for column in self.columns_ordered:
            axis = column.axis
            if axis not in self.axes_ordered:
                self.axes_ordered.insert(0, axis)
                axis.num = num
                num += 1
        num = 0
        for axis in self.axes_ordered:
            axis.num = num
            num += 1

    def jschart(self):
        print """
			window.chart = new Highcharts.StockChart({
			    chart: {
			        renderTo: '"""+self.id+"""'
			    },

			    rangeSelector: {
			        selected: 1
			    },

			    title: {
			        text: '"""+self.title+"""'
			    },
                            legend: {
                                enabled: true,
                                floating: false,
                                verticalAlign: "top",
                                x: 100,
                                y: 60,
                            },
                            tooltip: {
                                formatter: function() {
                                    var s = '<b>'+ Highcharts.dateFormat('%a, %d %b %Y %H:%M:%S', this.x) +'</b><br/>';
                                    s += commitMap[this.x].msg;
                                    $.each(this.points, function(i, point) {
                                        s += '<br/><span style="color:'+ point.series.color+';">'+ point.series.name +'</span>: '+point.y;
                                    });
                                    return s;
                                },
                                style: {
                                    whiteSpace: 'normal',
                                    width: '400px',
                                },
                            },
		            plotOptions: {
		                series: {
		                    events: {
		                        click: function(event) {
					    var lastpoint = null;
					    for (var i in this.data) {
					      if (event.point == this.data[i]) {
					        if (i > 0) lastpoint = this.data[i-1];
					        break;
					      }
					    }
					    if (lastpoint)
					      window.location = "http://os.inf.tu-dresden.de/~jsteckli/cgi-bin/cgit.cgi/nul/log/?qt=range&q="+commitMap[lastpoint.x].hash+'..'+commitMap[event.point.x].hash;
					    else
					      window.location = "http://os.inf.tu-dresden.de/~jsteckli/cgi-bin/cgit.cgi/nul/log/?id="+commitMap[event.point.x].hash;
					}
		                    }
		                }
		            },
			    yAxis: ["""
	for axis in self.axes_ordered:
            print "\t\t\t\t{"
            print "\t\t\t\t\tlineWidth: 1,"
            print "\t\t\t\t\tlabels: { align: 'right', x: -3 },"
            print "\t\t\t\t\ttitle: { text: '%s' }," % axis.getLabel()
            #print "\t\t\t\t\tplotBands: { from: %s, to: %s, color: '#eee' }," % (col.low, col.high)
            if axis.yrange_min: print "\t\t\t\t\tmin: %s," % axis.yrange_min
            if axis.yrange_max: print "\t\t\t\t\tmax: %s," % axis.yrange_max
            print "\t\t\t\t},"
        print """\t\t\t    ],

			    series: ["""
        num = 0
	for col in self.columns_ordered:
            print "\t\t\t\t{ name: '%s [%s]', yAxis: %d, data: [" % (col.name, col.units, col.axis.num)
            num += 1
            for row in self.rows:
                val = row[col.name]
                if val == None: val = "null"
                print "\t\t\t\t\t[%s, %s], " % (row.getDate(), val)
            print "\t\t\t\t]},"
        print """\t\t\t    ],
			});"""

class Graphs(dict):
    pass

graphs = Graphs()
commits = {}

date = time.localtime(time.time())

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

        match = re_commit.match(what)
        if match:
            date = time.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")
            commit = match.group(2)
            match = re_commithash.search(commit);
            if match:
                commithash = match.group(1)
            else:
                commithash = None
            commits[date] = (commit, commithash)

        (basename, ext) = os.path.splitext(os.path.basename(where))

        if what != "all": title = what
        else: title = basename
        try:
            graph = graphs[basename]
        except KeyError:
            graph = Graph(basename, title)
            graphs[basename] = graph
        continue

    match = re_perf.match(line)
    if match:
        perfstr = match.group(3)
        perf = perfstr.split()
        col = perf[0]
        try:
            val = float(perf[1])
        except ValueError:
            val = None
        try:
            units = perf[2]
            if '=' in units: units = None
        except:
            units = None
        if match.group(4) != "ok":
            val=None

        graph.addValue(date, col, val, units)

        match = re_perfaxis.search(perfstr)
        if match:
            graph.setAxis(col, match.group(1));

graphs = [g for g in graphs.values() if len(g.columns)]
graphs = sorted(graphs, key=lambda g: g.title.lower())

for g in graphs:
    g.findRanges()
    g.fixupAxisNumbers()

print """
<!DOCTYPE HTML>
<html>
    <head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
	<title>NUL Performance Plots</title>

	<script type="text/javascript" src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js"></script>
	<script type="text/javascript">
		var commitMap = {"""
for d in sorted(commits.iterkeys()):
    v = commits[d];
    print '\t\t\t%d: { msg: "%s", hash: "%s" },' % (1000*time.mktime(d), v[0].replace('"', '\\"'), str(v[1]).replace('"', '\\"'))
print """\t\t};
		$(function() {"""
for graph in graphs:
    graph.jschart()
print """
		});
	</script>
    </head>

    <body>
	<h1>NUL Performance Plots</h1>
	<script type="text/javascript" src="js/highstock.js"></script>
        <ul>
"""
for graph in graphs:
    print "	<li><a href='#%s'>%s</a></li>" % (graph.title, graph.title)
print "    </ul>"
for graph in graphs:
    print "	<h2><a name='%s'>%s</a></h2>" % (graph.title, graph.title)
    print '	<div id="%s" style="height: 400px"></div>' % graph.id
print """
    </body>
</html>
"""

# Local Variables:
# compile-command: "cat nul-nightly/nul_*.log|./wvperfpreprocess.py|./wvperf2html.py > graphs.html"
# End:
