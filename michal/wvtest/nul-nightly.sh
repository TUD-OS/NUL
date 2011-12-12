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
echo "Testing \"Compilation etc.\" in $0:"

CC=/home/sojka/gcc/gcc-4.6/bin/gcc
CXX=/home/sojka/gcc/gcc-4.6/bin/g++

echo sha1: $(sha1sum $CC)
$CC --version
echo sha1: $(sha1sum $CXX)
$CXX --version

cd build
git clean --quiet -fxd

cat <<EOF > ../kernel/contrib/nova-patches/remove-timestamp.patch
diff --git a/src/init.cpp b/src/init.cpp
index 36c0d95..c66feaf 100644
--- a/src/init.cpp
+++ b/src/init.cpp
@@ -63,7 +63,7 @@ void init (mword mbi)
     screen.init();
 
      // Now we're ready to talk to the world
-    printf ("\f%s: %s %s [%s]\n\n", version, __DATE__, __TIME__, COMPILER_STRING);
+    printf ("\f%s: %s %s [%s]\n\n", version, "??? ?? ????", "??:??:??", COMPILER_STRING);
 
     Idt::build();
     Gsi::setup();
EOF

scons target_cc=$CC target_cxx=$CXX NO_TIMESTAMP=1
make -C ../alexb/apps/libvirt || echo "! $0 libvirt build  FAILED"

find \( -name src -o -name .git -o -path ./contrib/nova -o -path ./.sconf_temp \) -prune -o \
     -type f ! -name '*.[oa]' ! -name '*.debug' ! -name .sconsign.dblite -print0 | xargs -0 sha1sum

PATH=$HOME/bin:$PATH

# Run the tests
novaboot --iprelay=on

# Reseting the machine just after power on confuses BIOS and causes it to ask some stupid question.
sleep 20

echo "! $0 compilation finished  ok"

ret=0
/home/sojka/nul/michal/wvtest/runall --server --iprelay || ret=1

novaboot --iprelay=off
date
trap - EXIT
exit $ret
