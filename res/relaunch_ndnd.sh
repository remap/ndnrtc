#!/bin/sh
ndnd-tlv-stop
rm -f /tmp/ndnd.log
ndnd-tlv-start
#ndnd2c add /ndn/edu/ucla/cs udp lioncub
echo "set route to lioncub"
ndndc add /ndn/edu/ucla/cs udp lioncub

hosts=$(cat /etc/hosts)
regex='(ndn\..*)'
for host in $hosts; do 
	if [[ "${host}" =~ $regex ]] ; then 
	    echo "set route to ${host}"
		ndndc add /ndn/edu/ucla/cs udp "${host}"
	fi; 
done;
