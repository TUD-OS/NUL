#!/bin/bash
set -e
PATH=$HOME/bin:$PATH
ret=0
log=$(date '+nul_%F_%T.log')

cd ~/nul

git fetch --quiet
git reset --quiet --hard origin/master
git clean --quiet -f
git submodule --quiet update --init
git submodule --quiet foreach --recursive 'git reset --quiet --hard && git clean --quiet -f'

if ! cmp $0 michal/wvtest/nul-nightly-cron.sh; then
    cp michal/wvtest/nul-nightly-cron.sh $0 && exec $0
fi

cd ~/nul-nightly
if ! nul-nightly.sh > $log 2>&1; then
    ret=1
    (
	cat <<EOF
Subject: NUL nighly build/test failed!
To: sojka@os

Pipe this mail to 'nul/michal/wvtest/wvtestrun cat' to easily find out
what failed.

EOF
	cat $log | tr -d '\015' #| iconv -f ASCII -t ASCII//IGNORE
	echo
    ) | /usr/sbin/sendmail -ti
fi

git add $log
git commit --quiet -m 'New nightly build log'

cat nul_*.log | wvperfpreprocess.py | wvperf2html.py > performance.html && mv performance.html ~/public_html/nul/ || exit 1
wvtest2html.py < $log > test-report.html && mv test-report.html ~/public_html/nul/ || exit 1

git push --quiet backup HEAD

exit $ret
