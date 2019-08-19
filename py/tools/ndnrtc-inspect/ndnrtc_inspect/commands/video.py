"""video command."""

import time, sys
from .base import CommandBase
from termcolor import colored
from json import dumps
from pyndn import Name, DelegationSet
from pycnl import Namespace, NamespaceState
from pycnl.generalized_object import GeneralizedObjectStreamHandler
from pycnl.generalized_object.content_meta_info import ContentMetaInfo
from .ndnrtc_pb2 import FrameMeta

class Video(CommandBase):
    def __init__(self, options, *args, **kwargs):
        CommandBase.__init__(self, options, args, kwargs)
        self.receivedSeq_ = None
        self.fout_ = open(self.options['--out'], 'wb')

    def run(self):
        if self.options['--start']:
            self.fetchGobjStream(self.options['<stream_prefix>'])
        else:
            self.fetchGobjStream(Name(self.options['<stream_prefix>']))

        while not self.mustQuit_:
            self.face_.processEvents()
            time.sleep(0.01)
        close(self.fout_)

    def fetchGobjStream(self, streamName):
        handlers = [None]

        def namespaceStateChanged(namespace, changedNamespace, state, clbckId):
            if state == NamespaceState.INTEREST_NETWORK_NACK or \
                 state == NamespaceState.DECRYPTION_ERROR or \
                 state == NamespaceState.ENCRYPTION_ERROR or \
                 state == NamespaceState.SIGNING_ERROR:
                self.log('unhandled namespace state: ', state, 'for',
                         changedNamespace.getName().toUri())
            elif state == NamespaceState.INTEREST_TIMEOUT:
                nOutstandingSequenceNumbers = handlers[0]._nRequestedSequenceNumbers - handlers[0]._nReportedSequenceNumbers
                self.log('interest timeout for',  changedNamespace.getName().toUri(),
                         '(outstanding ', nOutstandingSequenceNumbers, ')')

        def onNewFrame(sequenceNumber, contentMetaInfo, objectNamespace):
            if not (self.receivedSeq_ is None):
                if sequenceNumber < self.receivedSeq_:
                    self.printStats(objectNamespace, True, handler=handlers[0])
                else:
                    self.printStats(objectNamespace, handler=handlers[0])
            # else:
                # self.printMeta(contentMetaInfo)
            self.dumpFrame(objectNamespace)
            self.receivedSeq_ = sequenceNumber
            # free up memory
            objectNamespace._children = {}
            objectNamespace._sortedChildrenKeys = []

        stream = Namespace(streamName)
        stream.setFace(self.face_)
        ppSize = int(self.options['--pipeline'])
        self.log('fetching', stream.getName().toUri(), 'pipeline size', ppSize)
        GeneralizedObjectStreamHandler(stream, ppSize, onNewFrame).objectNeeded()
        stream.addOnStateChanged(namespaceStateChanged)

    def dumpFrame(self, frameNmspc):
        # we dump frame in "ndnrtc-codec"-friendly format, i.e.
        # first 4 bytes denote size of the encoded frame data
        # followed by this encoded frame data
        bytes = frameNmspc.obj.toBytes()
        size = len(bytes)
        self.fout_.write(size.to_bytes(4, 'little'))
        self.fout_.write(bytes)

    def printMeta(self, contentMetaInfo):
        frameMeta = FrameMeta()
        frameMeta.ParseFromString(contentMetaInfo.getOther().toBytes())
        self.logMultiline('start frame meta',
                          'content-type: '+str(contentMetaInfo.getContentType()),
                          'timestamp: '+str(contentMetaInfo.getTimestamp()),
                          'has_segments: '+str(contentMetaInfo.getHasSegments()),
                          frameMeta)

    def printStats(self, frameNmspc, outOfOrder = False, handler=None):
        name = frameNmspc.getName()
        warn = '[OUT OF ORDER]' if outOfOrder else ''
        msg = colored('received #'+str(name[-1].toSequenceNumber()), 'red')
        if handler:
            nOutstandingSequenceNumbers = handler._nRequestedSequenceNumbers - handler._nReportedSequenceNumbers
            msg += ' (outstanding #' + str(nOutstandingSequenceNumbers) + ')'
        line = ' >  {0} {1} {2:100}\r'.format(
                                        msg,
                                        name.toUri(),
                                        colored(warn, 'yellow'))
        sys.stdout.write(line)
