#!/bin/sh

TARGET=$1
URL=$2
REVISION=$3

rm -dRf "$TARGET" || exit 1
git clone "$URL" $TARGET || exit 1

( cd $TARGET && git checkout -b nul-local "$REVISION" ) || exit 1

shift 3

while [ $# -ne 0 ]; do
    ( cd $TARGET && git apply "$1" && git commit -am "$1" ) || exit
    shift
done

# EOF
