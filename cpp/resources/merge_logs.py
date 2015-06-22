# this script parses several log files and merges important data from them in one
# resulting log. this log provides organizes information in easy and readable form,
# allowing to see how interests and data for individual video segments have traveled from
# producer to consumer and back

# resulting log is organized like this:
#
# +---------------------+-------------------+-----------------------+-----------------------+-------------------+
# | Consumer actions	| Consumer buffer 	| Local daemon actions	| Remote daemon actions	| Producer actions	|
# +---------------------+-------------------+-----------------------+-----------------------+-------------------+
# | - expressed 		| - buffer short 	| - incoming interests:	| - incoming interests:	| - incoming 		|
# | interests:			| dump (see 		|      	-><f#>:<seg#>	|      	-><f#>:<seg#>	| interest:			|
# |  I-<f#>:<seg#>(<R>)	| frame-buffer.cpp)	| - forwarded interests:| - forwarded interests:|	I-<f#>:<seg#>	|
# |	f# - frame number	|					| 		--><f#>:<seg#>	| 		--><f#>:<seg#>	| - published data	|
# |	seg# - segment num.	|					| - arrived content:	| - arrived content:	|	D-<f#>:<seg#>	|
# | R - rtx flag		|					|		<-<f#>:<seg#>	|		<-<f#>:<seg#>	|					|
# |						|					| - forwarded contend:	| - forwarded contend:	|					|
# | - appended data 	|					|		<--<f#>:<seg#>	|		<--<f#>:<seg#>	|					|
# |		segments:		|					|						|						|					|
# |	 D-<f#>:<seg#>		|					|						|						|					|
# +---------------------+-------------------+-----------------------+-----------------------+-------------------+
#
# different sections are based on different log files entries:
#
# | - iqueue.log: "STAT"| - vbuffer.log:	| - ndnd.log:			| - ndnd.log:			| - vsender.log:	|
# | &"express" entries	| "STAT"&"shortdump"|	"interest_from"		|	"interest_from"		| "TRACE" &			|
# |						|	entries			|	"interest_to"		|	"interest_to"		| "interest"		|
# | - vbuffer.log: 		|					|	"content_from"		|	"content_from"		| "published"		|
# | "TRACE"&"appended"	|					|	"content_to"		|	"content_to"		|					|
# |		entries			|					|						|						|					|
# +---------------------+-------------------+-----------------------+-----------------------+-------------------+
#
# NOTES:
# - it is assumed that throughout consumers' logs timestamps are produced using the same
# 	clock
# - producer's timestamps across different logs were produced using the same clock
# - producer and consumer in any log should have entry with current time in unix form like
# 	this: "unix timestamp: XXXXXXXXXX.XXXXXX". this is needed to align ndnrtc logs and
# 	ndn daemon logs
# - time difference between two machines should be provided in the beginning of the script
# 	(probably using script parameters). otherwise, time difference is considered to be 0
#  	(machines have synchronized clocks)
#

# /ndn/edu/ucla/cs/ndnrtc/user/remap/streams/video0/vp8/frames/delta/14/data/%00%01/5/15/0/1

import fileinput
import re
# import multiprocessing
# from multiprocessing import Process, Lock, Event, Value
import threading
from threading import Thread, Lock, Event
import collections
import os

username = "loopback"
consumerRawLogPath = "raw"
producerRawLogPath = "raw"
consumerPath = "consumer/"+username
producerPath = "producer/"+username

test = False

consumerLog = "test.log" if test else "consumer-"+username+".log"
producerLog = "test.log" if test else "producer-"+username+".log"

consumerNtpInfo = "ntp.info"
producerNtpInfo = "ntp.info"

daemonFile = "ndnd.log"
vsenderLog = "vsender.log"

def getNtpDelay(file):
    "Retrieves ntp delay from the file which has output of ntpq -p command"
    pattern = re.compile('.*(?P<delay>\s+[-]?[0-9]+\.[0-9]+)(\s+[-]?[0-9]+\.[0-9]+){2}$')
    with open(file) as f:
        for line in f:
            m = pattern.match(line)
            if m:
                ntpDelay = float(m.group('delay'))
                return ntpDelay
    return 0.0

# these values should be updated form time to time
consumerNtpDelay = getNtpDelay(consumerRawLogPath+"/"+consumerNtpInfo)
producerNtpDelay = getNtpDelay(producerRawLogPath+"/"+producerNtpInfo)

print consumerNtpDelay
print producerNtpDelay

def checkPrefixLegal(prefix):
    return True;

def ndnHexToInt(ndnHexStr):
    pattern = re.compile('%(?P<higherByte>[0-9A-F]{2})%(?P<lowerByte>[0-9A-F]{2})')
    m = pattern.match(ndnHexStr)
    if m:
        intValue = int(m.group('higherByte')+m.group('lowerByte'), 16)
        return intValue
    else:
        return -1

class NdnObject:
    def __init__(self, prefix):
        "Extract information from the prefix and populates itself with the data"
        if checkPrefixLegal(prefix):
            prefixPattern = re.compile('[/A-Za-z0-9]*/frames/(?P<ftype>\w+)/?(?P<fno>[0-9]+)?/?(?P<dtype>\w+)?/?(?P<segno>[%A-F0-9]+)?.*')
            m = prefixPattern.match(prefix)
            self.frameType = None
            self.frameNo = -1
            self.dataType = None
            self.segNo = -1
            
            if m:
                if m.group('ftype'):
                    self.frameType = 'KEY' if m.group('ftype') == "key" else "DELTA"
                
                if m.group('fno'):
                    self.frameNo = int(m.group('fno'))
                
                if m.group('dtype'):
                    self.dataType = 'DATA' if m.group('dtype') == "data" else "PARITY"
                
                if m.group('segno'):
                    self.segNo = ndnHexToInt(m.group('segno'))
    
    def __str__(self):
        return str(self.frameType) + "-" + str(self.dataType)+" " + str(self.frameNo) + ":" + str(self.segNo)
    
    def __repr__(self):
        return self.__str__()
    
    @staticmethod
    def getInterestRegex(logEntry, component, token):
        "Compiles regular expression for parsing interest names from log files preceded with timestamps and token"
        pattern = '([.0-9]+) \['+logEntry+'\]\['+component+'\].*'+token+'\s([/A-Za-z0-9%]*)'
        return pattern


class Frame:
    def __init__(self, number, assembledLevel, deadline):
        self.number = int(number)
        self.assembledLevel = float(assembledLevel)
        self.deadline = int(deadline)
    
    def __str__(self):
        return str(self.number)+"("+str(self.deadline)+"|"+str(self.assembledLevel)+")"
    
    def __repr__(self):
        return self.__str__()

class BufferState:
    bufferDumpPattern = '\[((?P<fr0>[0-9]+)\((?P<lvl0>[01]\.?[0-9]+)\))?\s?(?P<nfr1>\+[0-9]+)?\s?((?P<kfr>[0-9]+)\((?P<kdl>[0-9]+)\|(?P<klvl>[01]\.?[0-9]*)\))?\s?(?P<nfr2>\+[0-9]+)?\s?(?P<fr1>[0-9]+)\((?P<dl1>[0-9]+)\|(?P<lvl1>[01]\.?[0-9]*)\)(?P<fr2>[0-9]+)\((?P<dl2>[0-9]+)\|(?P<lvl2>[01]\.?[0-9]*)\).*\]'
    
    def __init__(self, stringEntry):
        "Extract information from the buffer's short dump string"
        self.stringEntry = None
        self.frame0 = None
        self.frame1 = None
        self.frame2 = None
        self.keyFrame = None
        self.nFrames = None
        pattern = re.compile(BufferState.bufferDumpPattern)
        m = pattern.match(stringEntry)
        if m:
            self.stringEntry = stringEntry
            self.frame0 = Frame(m.group('fr0'), m.group('lvl0'), 0) if m.group('fr0') else None
            self.frame1 = Frame(m.group('fr1'), m.group('lvl1'), m.group('dl1')) if m.group('fr1') else None
            self.frame2 = Frame(m.group('fr2'), m.group('lvl2'), m.group('dl2')) if m.group('fr2') else None
            self.keyFrame = Frame(m.group('kfr'), m.group('klvl'), m.group('kdl')) if m.group('kfr') else None
            self.nFrames = 0
            
            if self.frame0:
                self.nFrames+=1
            
            if self.frame1:
                self.nFrames+=1
            
            if self.frame2:
                self.nFrames+=1
            
            if m.group('nfr1'):
                self.nFrames += int(m.group('nfr1'))
            
            if m.group('nfr2'):
                self.nFrames += int(m.group('nfr2'))
    
    def __str__(self):
        return self.stringEntry if self.stringEntry else "<n/a>"
    
    def __repr__(self):
        return self.__str__()
    
    @staticmethod
    def getBufferDumpRegex(logEntry, component, token):
        "Compiles regular expression for parsing buffer dumps from the log file"
        pattern = '(?P<ts>[0-9]+) \['+logEntry+'\]\['+component+'\].*'+token+'\s'+'('+BufferState.bufferDumpPattern+')'
        return pattern

# def TimeSynchronization:
#   def __init__(self):
#     self.timeSyncDict = {}
#     self.syncLock = Lock()
#
#   def updateTime(self, time)
#     threadName = threading.current_thread().name
#     with self.syncLock:
#       self.timeSyncDict[threadName] = time
#
#   def waitIfNotSynced(self, timeSlice)
#     threadName = threading.current_thread().name
#     with self.syncLock:
#       i = 0
#       prevTime = 0
#       diff = 0
#       tnameWithMaxTime = ''
#       maxTime = 0
#       if len(self.timeSyncDict) > 1:
#         for tname, time in self.timeSyncDict:
#           if i > 0:
#             diff += abs(prevTime - time)
#           if maxTime < time:
#             maxTime = time
#             tnameWithMaxTime = tname
#           prevTime = time
#           i += 1
#         # for
#         shouldWait = False
#         if diff > timeSlice and tnameWithMaxTime == threadName:
#           shouldWait = True

def initLogEntryDict(timestamp):
    "Initialize log entry dictionary"
    logEntry = {}
    logEntry['timestamp'] = timestamp
    logEntry['notes'] = ""
    logEntry['consumer'] = []
    logEntry['buffer'] = []
    logEntry['consumer-daemon'] = []
    logEntry['producer-daemon'] = []
    logEntry['producer'] = []
    return logEntry

def getSliceTime(timeslice, timestamp):
    return timestamp - timestamp%20

def getSliceForTimestamp(timeslice, timestamp):
    time = getSliceTime(timeslice, timestamp)
    try:
        logEntryBatch[time]
    except:
        logEntryBatch[time] = {}
        logEntryBatch[time]['mutex'] = Lock()
        logEntryBatch[time]['entries'] = {}
    return logEntryBatch[time]

def setSliceForTimestamp(timeslice, timestamp, logSlice):
    time = getSliceTime(timeslice, timestamp)
    logSlice['entries'] = collections.OrderedDict(sorted(logSlice['entries'].items()))
    logEntryBatch[time] = logSlice

def flushLog(filename, logBatch):
    print getThreadName() + " flushing " + str(len(logBatch)) + " entries"
    f = open(filename, 'a')
    sortedBatch = collections.OrderedDict(sorted(logEntryBatch.items()))
    for time, slice in sortedBatch.items():
        entries = slice['entries']
        f.write(str(time) + ":\n")
        #     print str(time) + ":"
        for timeshift, entry in entries.items():
            string = "+" + str(timeshift) + "\t"
            string += entry['consumer'] + "\t" if entry['consumer'] else "\t"
            string += entry['buffer'] + "\t" if entry['buffer'] else "\t"
            string += entry['consumer-daemon'] + "\t" if entry['consumer-daemon'] else "\t"
            string += entry['producer-daemon'] + "\t" if entry['producer-daemon'] else "\t"
            string += entry['producer'] if entry['producer'] else ""
            #       print string
            f.write(string+'\n')
    #     print "+" + str(timeshift) + "\t" + entry['consumer'] + "\t" + entry['buffer']
    f.close()

def getThreadName():
    return threading.current_thread().name

def checkAndFlushIfNeeded(timestamp, force):
    global flushReadyCounter
    global nActiveWorkers
    needFlush = False
    canFlush = False
    with flushLock:
        maxTs = max(logEntryBatch) if len(logEntryBatch) > 0 else 0
        #     print getThreadName()+" current ts: " + str(timestamp) + " size: "+str(len(logEntryBatch)) + " max ts: "+str(maxTs)
        if force or (len(logEntryBatch) >= maxBatchSize and timestamp >= max(logEntryBatch)):
            needFlush = True
            if flushReadyCounter == 0:
                flushEvent.clear()
            if flushReadyCounter < nActiveWorkers:
                flushReadyCounter += 1
            canFlush = (flushReadyCounter == nActiveWorkers)
    if needFlush:
        if canFlush:
            with flushLock:
                flushLog(resultLog, logEntryBatch)
                logEntryBatch.clear()
                flushReadyCounter = 0
                #         print getThreadName()+" flushed batch"
                flushEvent.set()
        else:
            print getThreadName()+" flushing: waiting for other threads to be ready"
            flushEvent.wait()
# if needFlush

def parseLog(file, timeslice, actionArray):
    print "parsing "+file+" in thread "+getThreadName()
    global nActiveWorkers
    timestamp = 0
    with open(file) as f:
        nActiveWorkers += 1
        for line in f:
            for action in actionArray:
                pattern = action['pattern']
                timeFunc = action['tfunc']
                actionFunc = action['func']
                m = pattern.match(line)
                if m:
                    timestamp = timeFunc(m)
                    checkAndFlushIfNeeded(timestamp, False)
                    logSlice = getSliceForTimestamp(timeslice, timestamp)
                    with logSlice['mutex']:
                        timeshift = timestamp%timeslice
                        try:
                            entry = logSlice['entries'][timeshift]
                        except:
                            entry = initLogEntryDict(timestamp)
                        entry[action['bucket']] = actionFunc(m)
                        logSlice['entries'][timeshift] = entry
                        setSliceForTimestamp(timeslice, timestamp, logSlice)
        # if len() else
        # if m
        # for action in actionArray
        # for line
        print getThreadName()+" finished parsing"
        checkAndFlushIfNeeded(timestamp, True)
        nActiveWorkers -= 1
        if nActiveWorkers == 0:
            flushLog(resultLog, logEntryBatch)
# with

def getSampleTimestamp(logfile):
    with open(logfile) as f:
        for line in f:
            pattern = re.compile('^(?P<timestamp>[0-9]+).*')
            m = pattern.match(line)
            if m:
                timestamp = m.group('timestamp')
                return timestamp
    return 0

def getTimeMap(logfile):
    unixTimePattern = re.compile('(?P<timestamp>[0-9]+) \[INFO \].*\sunix timestamp:\s(?P<unixtimestamp>[0-9]{10}\.[0-9]{6})')
    pattern = re.compile(unixTimePattern)
    with open(logfile) as f:
        for line in f:
            m = pattern.match(line)
            if m:
                timeMap = {}
                timeMap['time'] = m.group('timestamp')
                timeMap['unix'] = m.group('unixtimestamp')
                return timeMap
    return {}

maxBatchSize = 50
timeSlice = 20 # milliseconds
nActiveWorkers = 0
flushLock = Lock()
flushReadyCounter = 0
flushEvent = Event()

# this dictionary is used for synchronization between several workers
timeSyncDict = {}
syncLock = Lock()
syncEvent = Event()

# log entry batch dictionary as keys contains consumer's timestamps. consecutive
# timestamps are separated by timeSlice milliseconds
# each entry as a value has an array of dictionaries which can be created using
# initLogEntryDict this logEntryBatch is being filled simultaneously form 4 different log
# files and is flushed to the disk periodically
logEntryBatch = collections.OrderedDict()

consumerRawLogFile = consumerRawLogPath+"/"+consumerLog
producerRawLogFile = producerRawLogPath+"/"+producerLog
localDaemonLog = consumerRawLogPath+"/"+daemonFile
remoteDaemonLog = producerRawLogPath+"/"+daemonFile
resultLog = 'frame-trace.log'

parseInterests = {}
parseInterests['pattern'] = re.compile(NdnObject.getInterestRegex('STAT ', 'cchannel-iqueue', 'express'))
parseInterests['tfunc'] = lambda match: int(match.group(1))
parseInterests['bucket'] = 'consumer'
parseInterests['func'] = lambda match: "I->" + str(NdnObject(match.group(2)))

parseData = {}
parseData['pattern'] = re.compile(NdnObject.getInterestRegex('TRACE', 'vconsumer', 'data'))
parseData['tfunc'] = lambda match: int(match.group(1))
parseData['bucket'] = 'consumer'
parseData['func'] = lambda match: "D<-" + str(NdnObject(match.group(2)))

parseBuffer = {}
parseBuffer['pattern'] = re.compile(BufferState.getBufferDumpRegex('STAT ', 'vconsumer-buffer-pqueue', '.(pop|push)'))
parseBuffer['tfunc'] = lambda match: int(match.group(1))
parseBuffer['bucket'] = 'buffer'
parseBuffer['func'] = lambda match: match.group(2)+" "+str(BufferState(match.group(3)))

timeMapProducer = getTimeMap(producerRawLogFile)
timeMapConsumer = getTimeMap(consumerRawLogFile)

print timeMapProducer
print timeMapConsumer

producerTimeDiff = float(timeMapProducer['unix'])*1000-float(timeMapProducer['time'])
consumerTimeDiff = float(timeMapConsumer['unix'])*1000-float(timeMapConsumer['time'])

deltaMs = float(timeMapProducer['unix'])-float(timeMapConsumer['unix']) + (consumerNtpDelay-producerNtpDelay)
deltaMs = int(deltaMs*1000)
print "delta " + str(deltaMs)
print producerTimeDiff
print consumerTimeDiff

t1 = float(timeMapProducer['time'])
t2 = float(timeMapConsumer['time'])
# print t1
# print t2
# print deltaMs

producerConsumerTimeAdjustment = int(t2-t1+deltaMs)
# print producerConsumerTimeAdjustment

parseFrames = {}
parseFrames['pattern'] = re.compile(NdnObject.getInterestRegex('TRACE', 'vsender', 'published'))
parseFrames['tfunc'] = lambda match: int(match.group(1))+producerConsumerTimeAdjustment
parseFrames['bucket'] = 'producer'
parseFrames['func'] = lambda match: "P: " + str(NdnObject(match.group(2)))

f = open(resultLog, 'w')
f.write('')
f.close()

# parseLog(producerRawLogFile, timeSlice, [parseFrames])

if __name__ == '__main__':
    p1 = Thread(target = parseLog, args=(consumerRawLogFile, timeSlice, [parseInterests, parseData, parseBuffer],))
    p2 = Thread(target = parseLog, args=(producerRawLogFile, timeSlice, [parseFrames]))  
    p1.start()
    p2.start()
    p1.join()
    p2.join()

# parseLog(consumerRawLogFile, timeSlice, [parseInterests, parseData, parseBuffer])
# parseLog(producerRawLogFile, timeSlice, [parseFrames])

# sortedBatch = collections.OrderedDict(sorted(logEntryBatch.items()))
# 
# for time, slice in sortedBatch.items():
#   entries = slice['entries']
#   print str(time) + ":"
#   for timeshift, entry in entries.items():
#     print entry
#     print "+" + str(timeshift) + "\t" + entry['consumer'] + "\t" + entry['buffer']

# expressedInterests = parseInterests(rawLogFile,'STAT ', 'cchannel-iqueue', 'express')
# receivedSegments = parseInterests(rawLogFile, 'TRACE', 'vconsumer-buffer', 'appended')
# publishedSegments = parseInterests(producerPath+"/"+vsenderLog, 'TRACE', 'vsender', 'published')
