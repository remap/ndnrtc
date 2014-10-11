#!/bin/sh

RESDIR="../resources"
LIBDIR="../.libs"
RUNDIR=".."
APPNAME="ndnrtc-demo"

function assert_dir_access { 
	fail=0
    (cd ${1:?pathname expected}) || fail=1
    
    if [ "$fail" -eq "1" ]; then
    	echo ${2}
    	exit
    fi
}

assert_dir_access $RESDIR "    Can't find resources folder"
assert_dir_access $LIBDIR "    Please, run make && make ndnrtc-demo first"

PARAMSPATH="$(cd $RESDIR && pwd)/ndnrtc-new.cfg"
LIBPATH="$(cd $LIBDIR && pwd)/libndnrtc.dylib"


$RUNDIR/$APPNAME $LIBPATH $PARAMSPATH 2> stderr.out
