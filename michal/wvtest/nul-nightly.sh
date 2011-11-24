#!/bin/bash

_myexit() {
    code=$?
    cmd=$BASH_COMMAND
    echo >&2 "! $0 Command '$cmd' exit with code $code FAILED"
}

trap '_myexit' EXIT

set -e

cd ~/nul

ver="$(git describe --dirty) $(git log -n 1 --format='(%an: %s)')"
echo "Testing \"$(date "+%A %F %T"), commit: $ver\" in $0:"

CC=/usr/local/gcc/4.6/bin/gcc
CXX=/usr/local/gcc/4.6/bin/g++

echo sha1: $(sha1sum $CC)
$CC --version
echo sha1: $(sha1sum $CXX)
$CXX --version

cd build
scons target_cc=$CC target_cxx=$CXX
make -C ../alexb/apps/libvirt || echo "! $0 libvirt build  FAILED"
PATH=$HOME/bin:$PATH

# Run the tests
novaboot --iprelay=on

# Reseting the machine just after power on confuses BIOS and causes it to ask some stupid question.
sleep 20 

ret=0
/home/sojka/nul/michal/wvtest/runall --server --iprelay || ret=1

novaboot --iprelay=off

trap - EXIT
exit $ret
