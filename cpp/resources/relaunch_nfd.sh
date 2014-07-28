#!/bin/sh
DAEMON_START=nfd-start
DAEMON_STOP=nfd-stop
DAEMON_SET_ROUTE='nfdc register /ndn/edu udp://'

echo "*** stopping daemon..."
$DAEMON_STOP
rm -f /tmp/ndnd.log
echo "*** daemon stopped"

echo "*** starting daemon..."
$DAEMON_START
echo "*** daemon started"

hosts=$(cat /etc/hosts)
regex='^([0-9]+.*)(ndn\..*)$'

IFS=$'\n' 
for line in $hosts; do 
	if [[ "${line}" =~ $regex ]] ; then
		host="${BASH_REMATCH[2]}"
		ip="${BASH_REMATCH[1]}"
	    cmd="$DAEMON_SET_ROUTE$ip"
	    echo "*** set route to ${host} ("$cmd") "	    
		eval $cmd
	fi; 
done;
