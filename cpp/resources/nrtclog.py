import re
import threading
from threading import Thread, Lock, Event
import time

# This file contains some helpful functions for parsing NDN-RTC logs
# NDN-RTC log files are organized in the following manner:
# "<timestamp><TAB>[<log_level>]<TAB>[<component_name>]-<memory_address>: <log_message>"
# 
#   timestamp - application millisecond timestamp (not Unix-timestamp)l this timestamp has 
#               different clock base b/w separate app launches
#   log_level - one of the 6 logging levels: STAT, TRACE, DEBUG, INFO, WARN, ERROR
#   component_name -  name of the component where from where log message was originated
#   memory_address -  memory address of the component
#   log_message - log message originated from component component_name

#####
#####
#####
class LogParser:
  """General NDN-RTC log parser 
  """

  NdnRtcGeneralLogStringPattern = '(?P<timestamp>[0-9]+)\t\[(?P<level>STAT|TRACE|DEBUG|INFO|WARN|ERROR)\]\[(?P<component>[A-z0-9-\s]+)\]-\s*(?P<address>0x[0-9a-fA-F]+):\s*(?P<message>.*)'
  NdnRtcGeneralLogStringPatternExt = '(?P<timestamp>[0-9]+)\t\[(?P<level>LEVEL\s*)\]\[(?P<component>\s*COMPONENT)\]-\s*(?P<address>0x[0-9a-fA-F]+):\s*(?P<message>MSG)'
  FileMonitorIntervalMs = 10

  def __init__(self, pattern, matchCallback):
    self.pattern = pattern
    self.matchCallback = matchCallback
    self.errorCallback = None

  @staticmethod
  def ParseLog(logFileName, callback, parseErrorCallback):
    """Parses given log file and calls parsingCallback on for each individual log file line.
    The callback should have following interface:
    def parsingCallback(timestamp, log_level, component_name, memory_address, log_message)
    The error callback should take 1 parameter: line number with error
    """

    pattern = re.compile(LogParser.NdnRtcGeneralLogStringPattern)
    parser = LogParser(pattern, callback)
    parser.errorCallback = parseErrorCallback
    parser.processFile(logFileName, parser.processLine)

  @staticmethod
  def ParseLogExt(logFileName, callback, level, component, messageRegExp):
    """Parses given log file by applying provided regexps
    """

    pattern = LogParser.GetPattern(level, component, messageRegExp)
    parser = LogParser(pattern, callback)
    parser.processFile(logFileName, parser.processLine)

  @staticmethod
  def ParseLogLock(logFileName, callback, level, component, messageRegExp):
    """Lock on given log file and triggers callback anytime there was a new line written to it 
    which satisfies passed filters
    Returns a parser object
    """
    pattern = LogParser.GetPattern(level, component, messageRegExp)
    parser = LogParser(pattern, callback)
    parser.watchFile = True
    parser.tailing = True
    parser.tailingFile = open(logFileName)
    parser.tailThread = Thread(target=parser.tailFile)
    parser.watchThread = Thread(target=parser.watchNewLines)
    parser.watchThread.start()
    parser.tailThread.start()

    return parser

  @staticmethod
  def GetPattern(level, component, messageRegExp):
    patternString = LogParser.NdnRtcGeneralLogStringPatternExt.replace('LEVEL', level)
    patternString = patternString.replace('COMPONENT', component)
    patternString = patternString.replace('MSG', messageRegExp)
    return re.compile(patternString)

  ######
  def processFile(self, file, callback):
    """Reads a file line by line and triggers a callback for each line
    """
    lineNo = 0
    with open(file) as f:
      for line in f:
        lineNo = lineNo+1
        callback(lineNo, line)

  def processLine(self, lineNo, line):
    m = self.pattern.match(line)
    if m:
      self.matchCallback(m.group('timestamp'), m.group('level').strip(), m.group('component').strip(), m.group('address').strip(), m.group('message').strip())
    elif self.errorCallback:
      self.errorCallback(lineNo)

  def tailFile(self):
    print "tail"
    self.tailing = True
    f.seek(0, 2)
    while self.tailing:
      print "check"
      line = self.tailingFile.readline()
      if not line:
        time.sleep(LogParser.FileMonitorIntervalMs/1000)
        continue
      yield line

  def watchNewLines(self):
    print "watch"
    self.watchFile = True
    while self.watchFile:
      print "watch"
      line = (yield)
      self.processLine(-1, line)

  def stopWatchingFile(self):
    self.tailing = False
    self.watchFile = False

#####
#####
#####
class Frame:
  # This class can parse consumer-buffer output for frame entries
  FrameStringPattern = '(?P<frameType>[K|D])\s*,\s*(?P<seqNo>-?\d+)\s*,\s*(?P<playNo>-?\d+)\s*,\s*(?P<timestamp>-?\d+)\s*,\s*(?P<assembledLevel>-?\d+\.?\d*)%\s*\((?P<parityLevel>((-?\d*\.?\d*)|(nan)))%\)\s*,\s*(?P<pairedNo>-?\d+)\s*,\s*(?P<consistency>[CIHP]).*'
    
  def __init__(self, seqNo, assembledLevel):
    self.seqNo = int(seqNo)
    self.assembledLevel = float(assembledLevel)
    self.playNo = -1
    self.pairedNo = -1
    self.consistency = 'I'
    self.timestamp = -1
    self.parityLevel = -1
    self.frameType = 'U'

  def __init__(self, m):
    self.seqNo = int(m.group('seqNo'))
    self.assembledLevel = float(m.group('assembledLevel'))
    self.playNo = int(m.group('playNo'))
    self.pairedNo = int(m.group('pairedNo'))
    self.consistency = m.group('consistency')
    self.timestamp = int(m.group('timestamp'))
    self.parityLevel = float(m.group('parityLevel'))
    self.frameType = m.group('frameType')

  @staticmethod
  def initFromString(string):
    pat =".*("+Frame.FrameStringPattern+").*"
    pattern = re.compile(pat)
    m = pattern.match(string)
    f = None
    if m:
      f = Frame(m)
    return f
  
  def __str__(self):
      return "["+str(self.frameType)+", "+str(self.seqNo)+", "+str(self.playNo)+", "+str(self.timestamp)+", "+str(self.assembledLevel)+"% ("+str(self.parityLevel)+"%), "+str(self.pairedNo)+", "+str(self.consistency)+"]"
  
  def __repr__(self):
      return self.__str__()