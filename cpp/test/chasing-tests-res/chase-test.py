import fileinput
import re
import threading
from threading import Thread, Lock, Event
import collections
import os
import sys

token=".*"
component=".*"
keyword=".*"

if len(sys.argv) < 3:
  print "usage: "+__file__+" <id_num> <store_file>"
  exit(1)
  
idnum = sys.argv[1]
logfile = idnum+'-chase.log'
storeFile = sys.argv[2]

print "parsing " + logfile

nActiveWorkers = 0

chaseStory = {}
chaseStory['audio'] = 0
chaseStory['video'] = 0
chaseStory['audioreset'] = 0
chaseStory['videoreset'] = 0
chaseStory['latency'] = 0

def getThreadName():
    return threading.current_thread().name

def parseLog(file, actionArray):
    print "parsing "+file+" in thread "+getThreadName()
    global nActiveWorkers
    global chaseStory
    timestamp = 0
    stop = False
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
              if not actionFunc(timestamp, m):
                stop = True
                break
          if stop:
            break
        # for line
        print getThreadName()+" finished parsing"
        nActiveWorkers -= 1

def extractAudioChase(timestamp, match):
  global chaseStory
  if chaseStory['audio'] == 0:
    chaseStory['audio'] = int(match.group(2))
  return True

def extractVideoChase(timestamp, match):
  global chaseStory
  if chaseStory['video'] == 0:
    chaseStory['video'] = int(match.group(2))
  return True

def extractLatency(timestamp, match):
  global chaseStory
  chaseStory['latency'] = float(match.group(2))
  return True
  
def extractVideoReset(timestamp, match):
  global chaseStory
  chaseStory['videoreset'] += 1
  return True

def extractAudioReset(timestamp, match):
  global chaseStory
  chaseStory['audioreset'] += 1
  return True

actionArray = []

extract = {}
component = 'aconsumer-chase-est'
extract['pattern'] = re.compile('([.0-9]+)\s?\[\s*STAT \s*\]\s?\[\s*'+component+'\s*\]\s?.*finished in\s?(\d+).*')
extract['tfunc'] = lambda match: int(match.group(1))
extract['func'] = extractAudioChase
actionArray.append(extract)

extract2 = {}
component = 'vconsumer-chase-est'
extract2['pattern'] = re.compile('([.0-9]+)\s?\[\s*STAT \s*\]\s?\[\s*'+component+'\s*\]\s?.*finished in\s?(\d+).*')
extract2['tfunc'] = lambda match: int(match.group(1))
extract2['func'] = extractVideoChase
actionArray.append(extract2)

extract3 = {}
component = 'aconsumer-chase-est'
extract3['pattern'] = re.compile('([.0-9]+)\s?\[\s*STAT \s*\]\s?\[\s*'+component+'\s*\]\s?.*reset.*')
extract3['tfunc'] = lambda match: int(match.group(1))
extract3['func'] = extractAudioReset
actionArray.append(extract3)

extract4 = {}
component = 'vconsumer-chase-est'
extract4['pattern'] = re.compile('([.0-9]+)\s?\[\s*STAT \s*\]\s?\[\s*'+component+'\s*\]\s?.*reset.*')
extract4['tfunc'] = lambda match: int(match.group(1))
extract4['func'] = extractVideoReset
actionArray.append(extract4)

extract5 = {}
component = 'audio-playout'
extract5['pattern'] = re.compile('([.0-9]+)\s?\[\s*STAT \s*\]\s?\[\s*'+component+'\s*\]\s?.*latency(.*\d+\.\d+).*')
extract5['tfunc'] = lambda match: int(match.group(1))
extract5['func'] = extractLatency
actionArray.append(extract5)

parseLog(logfile, actionArray)

if chaseStory['audio'] != 0 and chaseStory['video'] != 0:
  with open(storeFile, 'a') as f:
    f.write(str(idnum) + '\t' + str(chaseStory['audio']) + '\t' + str(chaseStory['audioreset']) + '\t' + str(chaseStory['video']) + '\t' + str(chaseStory['videoreset']) + '\t' + str(chaseStory['latency']) + '\n')

print "parsing done"
