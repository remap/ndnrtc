import fileinput
import re
import threading
from threading import Thread, Lock, Event
import collections
import os
import sys

min=-1
max=sys.maxint
token=".*"
component=".*"
keyword=".*"

if len(sys.argv) < 4:
  print "usage: "+__file__+" <log_file> <min> <max> [token] [component] [keyword]"
  exit(1)
  
logfile = sys.argv[1]
min = -1
max = sys.maxint

if sys.argv[2] != "-":
  min = int(sys.argv[2])

if sys.argv[3] != "-":
  max = int(sys.argv[3])

if (len(sys.argv) >= 5):
  if sys.argv[4] != "-":
    token = sys.argv[4]

if (len(sys.argv) >= 6):
  if sys.argv[5] != "-":
    component = sys.argv[5]
    
if (len(sys.argv) >= 7):
  if sys.argv[6] != "-":
    keyword = sys.argv[6]

print "extracting logs from " + logfile + " between " + str(min) + " and " + str(max) + " token: " + token + " component: " + component + " keyword: "+keyword

nActiveWorkers = 0

def getThreadName():
    return threading.current_thread().name

def parseLog(file, actionArray):
    print "parsing "+file+" in thread "+getThreadName()
    global nActiveWorkers
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
# with

def extractFunc(timestamp, match):
  global min
  global max
#   print str(min) + " " + str(timestamp) + " " + str(max)
  if timestamp >= min and timestamp <= max:
    print match.group(0)
    return True
  if timestamp > max:
    return False
  return True

extract = {}
extract['pattern'] = re.compile('([.0-9]+)\s?\[\s*'+token+'\s*\]\s?\[\s*'+component+'\s*\]\s?.*'+keyword+'.*')
extract['tfunc'] = lambda match: int(match.group(1))
extract['func'] = extractFunc


parseLog(logfile, [extract])
