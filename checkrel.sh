#!/bin/sh

exit_code=0

echo "+ check all files in package.xml"

for i in $(ls *.c *.h)
do
	[ $i = config.h ] && continue
	grep -q $i package.xml || {
		echo "missing file $i in package.xml"
		exit_code=1
	}
done

for i in $(find tests -type f)
do
	i=$(basename $i)
	[ $i = .gitignore ] && continue
	grep -q $i package.xml || {
		echo "missing file $i in package.xml"
		exit_code=1
	}
done

exit $exit_code
