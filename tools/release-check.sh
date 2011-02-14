#!/bin/sh

echo Release Sanity Checker

if [ -f README.org ]; then
    echo
    echo "Compile README.org to README.txt and remove the former."
fi

echo
echo Checking for files with incomplete license plates ...
find . \( -name "*.[chS]" -or -name "*.cc" \) \! -exec grep -E -q "[Ee]conomic rights" {} \; -print
echo  ... done

echo
echo Checking for files with obsolete license plates ...
find . \( -name "*.[chS]" -or -name "*.cc" \) -exec grep -E -q "(is part of Vancouver|Vancouver.*is free sofware|Vancouver.*is distributed)" {} \; -print
echo  ... done

echo
echo Checking for temporary files ...
find . -name "*~"
echo  ... done

# EOF
