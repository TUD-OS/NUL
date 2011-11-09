#!/bin/bash

_myexit() {
    code=$?
    cmd=$BASH_COMMAND
    echo >&2 "! $0 Command '$cmd' exit with code $code FAILED"
}

trap '_myexit' EXIT

set -e

cd ~/nul

git reset --hard
git clean -f
git pull --quiet
git submodule update --init

ver="$(git describe) $(git log -n 1 --format='(%an: %s)')"
echo "Testing \"$(date "+%A %F %T"), commit: $ver\" in $0:"

cd build
scons target_cc=/usr/local/gcc/4.6/bin/gcc target_cxx=/usr/local/gcc/4.6/bin/g++

# Setup environment for libvirt tests
make -C ../alexb/apps/libvirt install DESTDIR=$PWD/libvirt
cp ../michal/imgs/passive/CAcertSrv.pem cacert.pem
PATH=$HOME/bin:$PWD/libvirt/usr/local/bin:$PATH
LD_LIBRARY_PATH=$PWD/libvirt/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export LD_LIBRARY_PATH

# Run the tests
novaboot --iprelay=on

# Reseting the machine just after power on confuses BIOS and causes it to ask some stupid question.
sleep 20 

ret=0
/home/sojka/nul/michal/wvtest/runall --server --iprelay || ret=1

novaboot --iprelay=off

trap - EXIT
exit $ret
