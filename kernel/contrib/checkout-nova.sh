#!/bin/sh

TARGET=$1
URL=$2
REVISION=$3

if test -d "$TARGET" && ( cd "$TARGET" && git rev-parse --is-inside-work-tree >/dev/null ) && \
   test -z "$(cd "$TARGET" && git rev-parse --show-prefix)"; then
    if ! ( cd "$TARGET" && git show --oneline $REVISION >/dev/null 2>&1 ); then
	( cd "$TARGET" && git fetch )
    fi
else
    rm -rf "$TARGET" || exit 1
    git clone "$URL" $TARGET || exit 1
    ( cd $TARGET && git branch nul-local ) || exit 1
fi

( cd $TARGET && git checkout nul-local && git reset --hard "$REVISION" ) || exit 1

shift 3

while [ $# -ne 0 ]; do
    ( cd $TARGET && git apply "$1" && git commit -am "$1" ) || exit
    shift
done

# EOF
