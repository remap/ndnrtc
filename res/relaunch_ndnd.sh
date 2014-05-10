#!/bin/sh

DAEMON_START=ndnd-tlv-start
DAEMON_STOP=ndnd-tlv-stop
DAEMON_SET_ROUTE='ndndc add /ndn/edu udp'

$DAEMON_STOP
rm -f /tmp/ndnd.log
$DAEMON_START

echo "ndn daemon started"

hosts=$(cat /etc/hosts)
regex='^[0-9]+.*(ndn\..*)$'

IFS=$'\n' 
for line in $hosts; do 
	if [[ "${line}" =~ $regex ]] ; then
		host="${BASH_REMATCH[1]}"
	    echo "set route to ${host}"
		eval $DAEMON_SET_ROUTE "${host}"
	fi; 
done;
