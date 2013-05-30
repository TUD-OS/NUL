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
date=$(date "+%F_%T")
echo "Testing \"$(date "+%A %F %T"), commit: $ver\" in $0:"
echo "Testing \"Compilation etc.\" in $0:"

CC=/usr/bin/gcc
CXX=/usr/bin/g++

echo sha1: $(sha1sum $CC)
$CC --version
echo sha1: $(sha1sum $CXX)
$CXX --version

cd build
git clean --quiet -fxd

# cat <<EOF > ../kernel/contrib/nova-patches/remove-timestamp.patch
# diff --git a/src/init.cpp b/src/init.cpp
# index 36c0d95..c66feaf 100644
# --- a/src/init.cpp
# +++ b/src/init.cpp
# @@ -63,7 +63,7 @@ void init (mword mbi)
#      screen.init();
 
#       // Now we're ready to talk to the world
# -    printf ("\f%s: %s %s [%s]\n\n", version, __DATE__, __TIME__, COMPILER_STRING);
# +    printf ("\f%s: %s %s [%s]\n\n", version, "??? ?? ????", "??:??:??", COMPILER_STRING);
 
#      Idt::build();
#      Gsi::setup();
# EOF

cp ../kernel/contrib/Chanage-serial-console-to-work-with-mmio-based-card-.patch ../kernel/contrib/nova-patches

export SCONSFLAGS="target_cc=$CC target_cxx=$CXX NO_TIMESTAMP=1 prepend_path=$HOME/bin"
scons
make -C ../alexb/apps/libvirt || echo "! $0 libvirt build  FAILED"

find \( -name src -o -name .git -o -path ./contrib/nova -o -path ./.sconf_temp \) -prune -o \
     -type f ! -name '*.[oa]' ! -name '*.debug' ! -name .sconsign.dblite -print0 | xargs -0 sha1sum

echo "! $0 compilation finished  ok"

echo "Testing \"Documentation build\" in $0:"
if scons doc; then
    echo "! $0 doc build  ok"
    rm -rf $HOME/public_html/nul/doc || echo "! $0 doc publish rm  FAILED"
    mv doc/html $HOME/public_html/nul/doc || echo "! $0 doc publish mv  FAILED"
else
    echo "! $0 doc build  FAILED"
fi

PATH=$HOME/bin:$PATH

ret=0
WVTEST_BACKUP_FAILED=/home/sojka/nul-nightly/failed/$date \
/home/sojka/nul/michal/wvtest/runall -I || ret=1

date
trap - EXIT
exit $ret
