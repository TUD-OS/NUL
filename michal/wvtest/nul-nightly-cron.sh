#!/bin/bash
set -e
log=$(date '+nul_%F_%T.log')
cd ~/nul-nightly
nul-nightly.sh > $log 2>&1 || :
ret=$?

if [[ $ret -ne 0 ]]; then
    (
	echo "Pipe this mail to 'nul/michal/wvtest/wvtestrun cat' to get more human readable formating."
	echo
	cat $log
    ) | mail -s "NUL nighly build & test failed!"
fi
cat $log  # Mail the log to me (backup)

cat nul_*.log | wvperf2html.py > performance.html && mv performance.html ~/public_html/nul/
