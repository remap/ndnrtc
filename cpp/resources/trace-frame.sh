#!/bin/sh

if [ $# -lt 2 ] ; then echo "usage: $0 <log> <frame_no> [delta|key] [diff]"; exit 1; fi
logFile="$1"
frameNo=$2

type="delta"
if [ $# -ge 3 ]; then
	if [ "$3" -eq "key" ] ; then
		type="key"
	else
		type="delta"
	fi
fi

useDiff=0
if [ $# -eq 4 ]; then
	if [ $4 -eq "diff" ] ; then
		useDiff=1
	else
		useDiff=0
	fi
fi

grepPipeline="cat ${logFile} | grep \"/ndn/edu/ucla/apps/ndnrtc/user/.*/streams/video0/vp8/frames/${type}/${frameNo}\"| grep -v \"vconsumer-buffer\" | grep -v \"new pit entry\" | grep -v \"pit hit\""

echo $logFile
if [ $useDiff -eq 1 ] ; then
	eval $grePipeline | time-diff.sh
else
	eval $grepPipeline
fi