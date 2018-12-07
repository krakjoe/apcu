#!/bin/sh
echo "+ check all files in package.xml"

for i in $(ls *.c *.h)
do
	[ $i == config.h ] && continue
	grep -q $i package.xml || echo missing $i
done
for i in $(find tests -type f)
do
	j=$(basename $i)
	[ $j == .gitignore ] && continue
	grep -q $j package.xml || echo missing $j
done
