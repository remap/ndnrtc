#!/bin/sh

PRODUCERS=`ls producer-*.log 2> /dev/null | sed 's/producer-//; s/\.log$//'`
CONSUMERS=`ls consumer-*.log 2> /dev/null | sed 's/consumer-//; s/\.log$//'`
DATE=`date`
LOGSFOLDER=${DATE//" "/"_"}".logs"
NDNDRC="~/.ndnx/ndndrc"

mkdir -p $LOGSFOLDER

function catAndGrep {
cat $1 | grep $2 > $3
}

function refineConsumer {
CFOLDER=$LOGSFOLDER/consumer/$1
CLOG=consumer-$1.log
echo "* refining logs for consumer "$1" ["$CFOLDER"] ..."

mkdir -p $CFOLDER

catAndGrep $CLOG "vconsumer-pipeliner" $CFOLDER/vpipeliner.log
echo "  pipeliner logs ready ["$CFOLDER/vpipeliner.log"]"

catAndGrep $CLOG "aconsumer-pipeliner" $CFOLDER/apipeliner.log
echo "  pipeliner logs ready ["$CFOLDER/apipeliner.log"]"

catAndGrep $CLOG "video-iqueue" $CFOLDER/viqueue.log
echo "  video interest queue logs ready ["$CFOLDER/viqueue.log"]"

catAndGrep $CLOG "audio-iqueue" $CFOLDER/aiqueue.log
echo "  audio interest queue logs ready ["$CFOLDER/aiqueue.log"]"

catAndGrep $CLOG "vconsumer-buffer" $CFOLDER/vbuffer.log
echo "  buffer logs ready ["$CFOLDER/vbuffer.log"]"

catAndGrep $CLOG "aconsumer-buffer" $CFOLDER/abuffer.log
echo "  buffer logs ready ["$CFOLDER/abuffer.log"]"

catAndGrep $CLOG "vconsumer\]" $CFOLDER/vconsumer.log
echo "  consumer logs ready ["$CFOLDER/vconsumer.log"]"

catAndGrep $CLOG "aconsumer\]" $CFOLDER/aconsumer.log
echo "  consumer logs ready ["$CFOLDER/aconsumer.log"]"

catAndGrep $CLOG "vconsumer-buffer-pqueue\]" $CFOLDER/vpqueue.log
echo "  playout queue logs ready ["$CFOLDER/vpqueue.log"]"

catAndGrep $CLOG "aconsumer-buffer-pqueue\]" $CFOLDER/apqueue.log
echo "  playout queue logs ready ["$CFOLDER/apqueue.log"]"

catAndGrep $CLOG "video-playout\]" $CFOLDER/vplayout.log
echo "  playout logs ready ["$CFOLDER/vplayout.log"]"

catAndGrep $CLOG "audio-playout\]" $CFOLDER/aplayout.log
echo "  playout logs ready ["$CFOLDER/aplayout.log"]"

catAndGrep $CLOG "\[STAT.\]\[.*aconsumer-chase-est\]" $CFOLDER/achase.stat.log
echo "  chase statistics ready ["$CFOLDER/achase.stat.log"]"

catAndGrep $CLOG "\[STAT.\]\[.*vconsumer-chase-est\]" $CFOLDER/vchase.stat.log
echo "  chase statistics ready ["$CFOLDER/vchase.stat.log"]"

catAndGrep $CLOG "\[STAT.\]" $CFOLDER/all.stat.log
echo "  all statistics ready ["$CFOLDER/all.stat.log"]"
}

function refineProducer {
PFOLDER=$LOGSFOLDER/producer/$1
PLOG=producer-$1.log

echo "* refining logs for producer "$1" ["$PFOLDER"]..."

mkdir -p $PFOLDER

catAndGrep $PLOG "vsender\]" $PFOLDER/vsender.log
echo "  producer logs ready ["$PFOLDER/vsender.log"]"

catAndGrep $PLOG "STAT.\]\[vsender\]" $PFOLDER/vsender.stat.log
echo "  producer statistics ready ["$PFOLDER/vsender.stat.log"]"

catAndGrep $PLOG "asender\]" $PFOLDER/asender.log
echo "  producer logs ready ["$PFOLDER/asender.log"]"

catAndGrep $PLOG "\[STAT.\]\[.*asender\]" $PFOLDER/asender.stat.log
echo "  producer statistics ready ["$PFOLDER/asender.stat.log"]"
}

echo "copying logs into ["$LOGSFOLDER/raw"]"
mkdir -p $LOGSFOLDER/raw
cp *.log $LOGSFOLDER/raw
cp *.cfg $LOGSFOLDER/raw
echo "`ntpq -p`" >> $LOGSFOLDER/raw/ntp.info

for consumer in $CONSUMERS
do
refineConsumer $consumer
done

for producer in $PRODUCERS
do
refineProducer $producer
done

echo "preparing logs archive..."

NDNDLOG="/tmp/ndnd.log"

if [ -e $NDNDRC ]; then
    LOGENTRY=`cat $NDNDRC | grep "NDND_LOG="`
    NDNDLOG=${LOGENTRY#NDND_LOG=}
fi

if [ -e $NDNDLOG ]; then
SIZE=`du -h $NDNDLOG`
read -p "copy ndnd log? ($SIZE) (y/n)[n] " -n 1
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
echo "copying ndnd log. this may take a while..."
cp -v /tmp/ndnd.log $LOGSFOLDER/raw
fi
fi

echo "compressing logs..."
tar -czf $LOGSFOLDER.tar.gz $LOGSFOLDER

echo "cd $LOGSFOLDER" | pbcopy
echo "logs are stored in "$LOGSFOLDER.tar.gz