#!/bin/bash
set -e
PATH=$HOME/bin:$PATH
ret=0
log=$(date '+nul_%F_%T.log')

cd ~/nul
umask 022

git fetch --quiet
git stash --quiet # Save any chnages made by humans e.g. some testing
git checkout --quiet master
git reset --quiet --hard origin/master
git clean --quiet -fxd > /dev/null
git submodule --quiet foreach --recursive 'git stash --quiet; git clean --quiet -fxd'
git submodule --quiet update --init

if ! cmp $0 michal/wvtest/nul-nightly-cron.sh; then
    cp michal/wvtest/nul-nightly-cron.sh $0 && exec $0
fi

cd ~/nul-nightly
if nul-nightly.sh > $log 2>&1; then
    ( cd ~/nul; git push --quiet -f origin master:tested ) || :
else
    ret=1
    (
	cat <<EOF
Subject: NUL nightly build/test failed!
To: commits-nul@os

Full log can be found at erwin:$PWD/$log

EOF
	export COLUMNS=100
	cat $log | tr -d '\015' | wvformat --before-failure=10 | wvwrap #| iconv -f ASCII -t ASCII//IGNORE
    ) | /usr/sbin/sendmail -ti
fi

git add $log
git commit --quiet -m 'New nightly build log'

cat nul_*.log | wvperfpreprocess.py | wvperf2html.py > performance.html && mv performance.html ~/public_html/nul/ || exit 1
wvtest2html.py ~/public_html/nul/test-report < $log || exit 1

git push --quiet backup HEAD
