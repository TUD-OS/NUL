#!/bin/sh

TAGFILE=TAGS

rm -f "$TAGFILE"

exec find . -type f \( -name "*.[ch]" -or -name "*.cc" \) -exec etags -o "$TAGFILE" --append {} \;
