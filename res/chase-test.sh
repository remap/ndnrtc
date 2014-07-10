#!/bin/sh

TESTPROG=chasing-test
PYPROG=chase-test.py
PYTHON=python
IDNUMFILE=test.id
STOREFILE=chasing.store

while [ 1 ] 
do
	sleep 7
	./$TESTPROG
	if [ $? -eq 0 ]; then
		$PYTHON $PYPROG `cat $IDNUMFILE` $STOREFILE
	fi
done
