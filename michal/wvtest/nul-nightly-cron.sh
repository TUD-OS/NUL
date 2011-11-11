#!/bin/bash
set -e
PATH=$HOME/bin:$PATH
ret=0
log=$(date '+nul_%F_%T.log')
cd ~/nul-nightly
if ! nul-nightly.sh > $log 2>&1; then
    ret=1
    (
	echo "Pipe this mail to 'nul/michal/wvtest/wvtestrun cat' to get more human readable formating."
	echo
	cat $log | tr -d '\015' | iconv -f ASCII -t ASCII//IGNORE
	echo
    ) | mail -s 'NUL nighly build/test failed!' sojka@os
fi
cat $log  # Mail the log to me (backup)

cat nul_*.log | wvperf2html.py > performance.html && mv performance.html ~/public_html/nul/ || exit 1

git add --quiet $log
git commit --quiet -m 'New nightly build log'
git push --quiet backup HEAD

exit $ret
