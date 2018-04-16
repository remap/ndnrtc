#!/bin/bash

if [ $START_CLIENT = "yes" ]; then
    nfd > $NFD_LOG 2>&1 &
    sleep 2 # allow NFD to start
    ./ndnrtc-client -c $CONFIG_FILE -s $SIGNING_IDENTITY -p $POLICY_FILE -t $RUNTIME -i $INSTANCE_NAME -n $STAT_SAMPLING -v
else
    nfd > $NFD_LOG 2>&1
fi