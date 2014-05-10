#!/bin/sh

i=0
read line1ts line1rest

while read line2ts line2rest; do
	if [ $i -ne 0 ]; then
		diff=$((line2ts-line1ts))
		diffSec=$((diff/1000))
		echo "\t^ ${diffSec} sec, ${diff} msec"
		echo "${line2ts} ${line2rest}"
	
		line1ts=$line2ts
		line1rest="${line2rest}" 
	else
		echo "${line1ts}\t${line1rest}"	
	fi
	let i++
done