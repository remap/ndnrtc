import fileinput
import re
import threading
from threading import Thread, Lock, Event
import collections
from collections import OrderedDict
import os
import sys

if len(sys.argv) < 2:
  print "usage: "+__file__+" <stat_file>"
  exit(1)
  
statfile = sys.argv[1]

headerPrefix = "*** "
linePrefix = "    "
captionLen = 30

SYMBOL_SEG_RATE="sr"
SYMBOL_INTEREST_RATE="ir"
SYMBOL_PRODUCER_RATE="rate"
SYMBOL_JITTER_TARGET="jt"
SYMBOL_JITTER_ESTIMATE="je"
SYMBOL_JITTER_PLAYABLE="jp"
SYMBOL_INRATE="in"
SYMBOL_NREBUFFER="nreb"

SYMBOL_NPLAYED="npb"
SYMBOL_NPLAYEDKEY="npbk"

SYMBOL_NSKIPPEDNOKEY="nskipk"
SYMBOL_NSKIPPEDINC="nskipi"
SYMBOL_NSKIPPEDINCKEY="nskipik"
SYMBOL_NSKIPPEDGOP="nskipg"

SYMBOL_NACQUIRED="nacq"
SYMBOL_NACQUIREDKEY="nacqk"

SYMBOL_NDROPPED="ndrop"
SYMBOL_NDROPPEDKEY="ndropk"

SYMBOL_NASSEMBLED="nasm"
SYMBOL_NASSEMBLEDKEY="nasmk"

SYMBOL_NRESCUED="nresc"
SYMBOL_NRESCUEDKEY="nresck"

SYMBOL_NRECOVERED="nrec"
SYMBOL_NRECOVEREDKEY="nreck"

SYMBOL_NINCOMPLETE="ninc"
SYMBOL_NINCOMPLETEKEY="ninck"

SYMBOL_NREQUESTED="nreq"
SYMBOL_NREQUESTEDKEY="nreqk"

SYMBOL_NRTX="nrtx"
SYMBOL_AVG_DELTA="ndelta"
SYMBOL_AVG_KEY="nkey"
SYMBOL_RTT_EST="rtt"
SYMBOL_NINTRST="nint"
SYMBOL_NDATA="ndata"
SYMBOL_NTIMEOUT="nto"

shortStat = OrderedDict([ ("Requested",SYMBOL_NREQUESTED), ("Acquired",SYMBOL_NACQUIRED), ("Played",SYMBOL_NPLAYED),
("Interests sent",SYMBOL_NINTRST), ("Data received",SYMBOL_NDATA), ("Timeouts",SYMBOL_NTIMEOUT),
("Rebufferings",SYMBOL_NREBUFFER), ("Retransmissions",SYMBOL_NRTX)])

fullStat = OrderedDict([ ("Requested",SYMBOL_NREQUESTED), ("Requested KEY",SYMBOL_NREQUESTEDKEY), ("Acquired",SYMBOL_NACQUIRED), ("Acquired KEY",SYMBOL_NACQUIREDKEY), 
  ("Played",SYMBOL_NPLAYED), ("Played KEY",SYMBOL_NPLAYEDKEY), 
("Assembled",SYMBOL_NASSEMBLED), ("Assembled KEY",SYMBOL_NASSEMBLEDKEY), ("Rescued",SYMBOL_NRESCUED), ("Rescued KEY",SYMBOL_NRESCUEDKEY), 
("Recovered",SYMBOL_NRECOVERED), ("Recovered KEY",SYMBOL_NRECOVEREDKEY), ("Incomplete",SYMBOL_NINCOMPLETE), ("Incomplete KEY",SYMBOL_NINCOMPLETEKEY),
("Skipped (no key)",SYMBOL_NSKIPPEDNOKEY), ("Skipped incomplete",SYMBOL_NSKIPPEDINC), ("Skipped incomplete KEY",SYMBOL_NSKIPPEDINCKEY), ("Skipped (invalid GOP)",SYMBOL_NSKIPPEDGOP),
("Outdated",SYMBOL_NDROPPED), ("Outdated KEY",SYMBOL_NDROPPEDKEY),
("Interests sent",SYMBOL_NINTRST), ("Data received",SYMBOL_NDATA), ("Timeouts",SYMBOL_NTIMEOUT),
("Rebufferings",SYMBOL_NREBUFFER), ("Retransmissions",SYMBOL_NRTX )])

statPostProcRules = {"Acquired":["/%","Requested"], "Played":["/%","Acquired"], "Data received":["/%","Interests sent"], "Timeouts":["/%","Interests sent"],
"Incomplete":["/%","Acquired"], "Skipped incomplete":["/%","Acquired"], "Skipped (no key)":["/%","Acquired"], "Skipped (invalid GOP)":["/%", "Acquired"],
"Assembled":["/%","Requested"], "Rescued":["/%","Requested"], "Recovered":["/%","Requested"]}

for text in shortStat:
  if captionLen < len(text):
    captionLen = len(text)+len(linePrefix)
for text in fullStat:
  if captionLen < len(text):
    captionLen = len(text)+len(linePrefix)

def getThreadName():
    return threading.current_thread().name

def printTestDuration(file):
  global headerPrefix
  startTimeMs = 0
  endTimeMs = 0
  pattern = re.compile('([.0-9]+)\s?\[\s*'+token+'\s*\]\s?\[\s*'+component+'\s*\]\s?.*'+keyword+'.*')  
  with open(file) as f:
    for line in f:
      m = pattern.match(line)
      if m:
        startTimeMs = int(m.group(1))
        break
    for line in reversed(list(f)):
      m = pattern.match(line)
      if m:
        endTimeMs = int(m.group(1))
        break
  if startTimeMs != endTimeMs and startTimeMs < endTimeMs:
    duration = endTimeMs-startTimeMs
    print headerPrefix+"Test duration: "+str(int(duration/10./60.)/100.)+"min "+str(int(duration/10)/100.)+"sec "+str(duration)+"ms"
    return True
  return False

def parseStat(file, actionArray):
    timestamp = 0
    stop = False
    with open(file) as f:
        for line in reversed(f.readlines()):
          for action in actionArray:
            pattern = action['pattern']
            timeFunc = action['tfunc']
            actionFunc = action['func']
            m = pattern.match(line)
            if m:
              timestamp = timeFunc(m)
              if actionFunc(timestamp, m):
                stop = True
                break
          if stop:
            break
        # for line
# with

def mineStats(statFields, statVariables):
  minedStat = statFields
  for text, symbol in statFields.iteritems():
    getVar = False
    hasFound = False
    for var in statVariables:
      if getVar:
        minedStat[text] = var
        getVar = False
      else:
        if var == symbol:
          getVar = True
          hasFound = True
    if not hasFound:
      minedStat[text]="n/a"
  return minedStat

def binaryOp(arg1, arg2, op):
  result = {
  "+": lambda a,b: a+b,
  "-": lambda a,b: a-b,
  "/": lambda a,b: int(a/b*100)/100.,
  "/%": lambda a,b: int(a/b*10000)/100.,   
  "*": lambda a,b: a*b
  }[op](arg1,arg2)
  return result

def printStat(statFields, statVariables):
  global linePrefix
  global captionLen
  global statPostProcRules

  minedVariables = mineStats(statFields, statVariables);
  for text, value in minedVariables.iteritems():
    sys.stdout.write(linePrefix+text+":")
    for i in range(0, captionLen-len(linePrefix)-len(text)):
      sys.stdout.write(" ")
    sys.stdout.write('\t')
    sys.stdout.write(value)
    if statPostProcRules.has_key(text):
      arg1 = minedVariables[text]
      arg2 = minedVariables[statPostProcRules[text][1]]
      op = statPostProcRules[text][0]
      print "\t"+str(binaryOp(float(arg1), float(arg2), op))
    else:
      print


def statFunc(timestamp, match):
  global headerPrefix
  global shortStat
  global fullStat
  statVariables = re.split(r'\t+', match.group(0))
  print headerPrefix+"Short summary:"
  printStat(shortStat, statVariables)
  print
  print headerPrefix+"Full summary:"
  printStat(fullStat, statVariables)
  return True

token="STAT"
component=".*"
keyword=".*"

if printTestDuration(statfile):
  makestat = {}
  makestat['pattern'] = re.compile('([.0-9]+)\s?\[\s*'+token+'\s*\]\s?\[\s*'+component+'\s*\]\s?.*'+keyword+'.*')
  makestat['tfunc'] = lambda match: int(match.group(1))
  makestat['func'] = statFunc
  parseStat(statfile, [makestat])

