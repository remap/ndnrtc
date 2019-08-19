"""frame command."""

import time, sys
from .base import CommandBase
from json import dumps
from pyndn import Name, Face, DelegationSet
from pycnl import Namespace, NamespaceState
from pycnl.generalized_object.content_meta_info import ContentMetaInfo
from .ndnrtc_pb2 import LiveMeta, StreamMeta, FrameMeta

class Frame(CommandBase):
    def __init__(self, options, *args, **kwargs):
        CommandBase.__init__(self, options, args, kwargs)
        self.segments_ = { 'seg-data': 0, 'seg-parity': 0 }
        # check if any of the specific meta flags are present
        # if not -- allow fetch all
        fetchAll = False
        if not (options['--meta'] or
                options['--manifest'] or
                options['--segments'] or
                options['--parity']):
            fetchAll = True
        if fetchAll:
            self.options['--meta'] = True
            self.options['--manifest'] = True
            self.options['--segments'] = True
            self.options['--parity'] = True
            self.fetched_ = {
                '_meta' : False, '_manifest' : False,
                'd-segments' : False, 'p-segments': False
                }
        else:
            self.fetched_ = {}
            if self.options['--meta']: self.fetched_['_meta'] = False
            if self.options['--manifest']: self.fetched_['_manifest'] = False
            if self.options['--segments']: self.fetched_['d-segments'] = False
            if self.options['--parity']: self.fetched_['p-segments'] = False

    def hasFetchedAll(self):
        fetchedAll = True
        for k,v in self.fetched_.items():
            fetchedAll = fetchedAll & v
        return fetchedAll

    def run(self):
        if self.options['<frame_prefix>'] == '-':
            framePrefix = sys.stdin.readline()
        else:
            framePrefix = self.options['<frame_prefix>']
        self.inspectFrame(Name(framePrefix))
        while not self.hasFetchedAll() and not self.mustQuit_:
            self.face_.processEvents()
            time.sleep(0.01)

    def inspectFrame(self, framePrefix):
        def onFrameMeta(metaNmspc):
            self.fetched_['_meta'] = True
            contentMetaInfo = ContentMetaInfo()
            contentMetaInfo.wireDecode(metaNmspc.getData().getContent().toBytes())
            frameMeta = FrameMeta()
            frameMeta.ParseFromString(contentMetaInfo.getOther().toBytes())
            if self.options['--parity']:
                nParity = frameMeta.parity_size
                def onParitySegment(segNmspc):
                    self.segments_['seg-parity'] += 1
                    if self.segments_['seg-parity'] == nParity:
                        self.fetched_['p-segments'] = True
                    self.logMultiline('parity segment',
                                      'segment #' + str(segNmspc.getName()[-1].toSegment()))
                    self.logPacketInfo(segNmspc.getData())
                for i in range(0, nParity):
                    p = Name(framePrefix).append('_parity').appendSegment(i)
                    self.fetchPacket(p, onParitySegment)
                if nParity == 0:
                    self.fetched_['p-segments'] = True
            if self.options['--meta']:
                self.logMultiline('meta',
                                'seq #: '+str(metaNmspc.getName()[-2].toSequenceNumber()),
                                'content-type: '+str(contentMetaInfo.getContentType()),
                                'timestamp: '+str(contentMetaInfo.getTimestamp()),
                                'has_segments: '+str(contentMetaInfo.getHasSegments()),
                                frameMeta)
                self.logPacketInfo(metaNmspc.getData())

        def onFrameManifest(manifestNmspc):
            self.fetched_['_manifest'] = True
            manifestSize = 32
            nManifests = int(len(manifestNmspc.obj)/manifestSize)
            lines = []
            idx = 0
            payload = manifestNmspc.obj.toBytes()
            for i in range(0, nManifests):
                manifest = payload[idx:idx+manifestSize]
                lines.append(str(i) + ': 0x' + ''.join('{:02x}'.format(x) for x in manifest))
                idx += manifestSize
            self.logMultiline('manifest',
                              'total segments: ' + str(nManifests),
                              *lines)
            self.logPacketInfo(manifestNmspc.getData())

        def onFirstSegment(segmentNmspc):
            nSegments = segmentNmspc.getData().getMetaInfo().getFinalBlockId().toSegment()+1
            def onSegment(segmentNmspc):
                self.segments_['seg-data'] +=1
                if self.segments_['seg-data'] == nSegments:
                    self.fetched_['d-segments'] = True
                self.logMultiline('data segment',
                                  'segment #' + str(segmentNmspc.getName()[-1].toSegment()))
                self.logPacketInfo(segmentNmspc.getData())
            for s in range(1, nSegments):
                self.fetchPacket(Name(segmentNmspc.getName().getPrefix(-1)).appendSegment(s), onSegment)
            onSegment(segmentNmspc)

        if self.options['--meta'] or self.options['--parity']:
            self.fetchPacket(Name(framePrefix).append("_meta"), onFrameMeta)
        if self.options['--manifest']:
            self.fetchPacket(Name(framePrefix).append("_manifest"), onFrameManifest)
        if self.options['--segments']:
            self.fetchPacket(Name(framePrefix).appendSegment(0), onFirstSegment)
