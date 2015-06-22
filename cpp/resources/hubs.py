import fileinput
import re
import collections
import os
import sys

class Hub:
	HubStringPattern='(?P<ip>[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)\s+(?P<name>[A-z0-9.-]+)\s+(?P<prefix>[A-z0-9/%]+)'

	def __init__(self, name, ip, prefix):
		self.name = name
		self.ip = ip
		self.prefix = prefix

	@staticmethod
	def fromString(string):
		hub=None
		pattern = re.compile(Hub.HubStringPattern)
		m = pattern.match(string)
		if m:
			hub = Hub(m.group('name'), m.group('ip'), m.group('prefix'))
		return hub
	
	def __str__(self):
		return "name: "+self.name+" ip: "+self.ip+" prefix: "+self.prefix

	def __repr__(self):
		return self.__str__()


def readHubs(hubsFile):
	hubs=[]
	with open(hubsFile) as f:
		for line in f:
			hub = Hub.fromString(line)
			if hub != None:
				hubs.append(hub)
	return hubs

def printHosts(hubs):
	for hub in hubs:
		print hub.ip+"\t"+hub.name
	return 0

def printNfdc(hubs):
	for hub in hubs:
		print "nfdc register "+hub.prefix+" udp://"+hub.ip
	return 0

def lookup(hubs, substr):
	for hub in hubs:
		if hub.name.find(substr) != -1 or hub.prefix.find(substr) != -1 or hub.ip.find(substr) != -1:
			print hub
	return 0

def getIp(hubs, name):
	for hub in hubs:
		if hub.name.find(name) != -1:
			return hub.ip
	return ''

def getName(hubs, ip):
	for hub in hubs:
		if hub.ip == ip:
			return hub.name
	return ''

def getPrefix(hubs, nameOrIp):
	for hub in hubs:
		if hub.ip.find(nameOrIp) != -1 or hub.name.find(nameOrIp) != -1:
			return hub.prefix
	return ''

# *****************************************************************************
if __name__ == '__main__':
	if len(sys.argv) < 3:
		print "usage: "+__file__+" hubs_file <cmd> [arg1,...]"
		print	"	cmd:"
		print "		hosts - reads hubs_file and prints it's contents in /etc/hosts format (ie. 'ip<tab>alias')"
		print "		nfdc - reads hubs_file and prints 'nfdc register' commands for each prefix with corresponding ip"
		print "		lookup <substr> - searches for substr in name or prefix and prints found hubs"
		print "		ip <name> - returns ip address corresponding to a given name"
		print "		name <ip> - returns name corresponding to a given ip address"
		print "		prefix <ip|name> - returns prefix corresponding to a given ip or name"
		exit(1)

	hubsFile = sys.argv[1]
	cmd  = sys.argv[2]

	arg1 = None
	if (len(sys.argv) >= 4):
		arg1 = sys.argv[3]

	# "switch" cmd:
	res = {
	'hosts' : lambda file, arg1: printHosts(readHubs(file)),
	'nfdc' : lambda file, arg1: printNfdc(readHubs(file)),
	'lookup' : lambda file, arg1: lookup(readHubs(file), arg1),
	'ip' : lambda file, arg1: getIp(readHubs(file), arg1),
	'name' : lambda file, arg1: getName(readHubs(file), arg1),
	'prefix': lambda file, arg1: getPrefix(readHubs(file), arg1)
	}[cmd](hubsFile, arg1)

	if res != 0:
		print res