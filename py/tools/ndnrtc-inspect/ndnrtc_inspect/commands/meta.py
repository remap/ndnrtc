"""meta command."""

import time
from .base import CommandBase
from json import dumps
from pyndn import Name, Face, DelegationSet
from pycnl import Namespace, NamespaceState
from .ndnrtc_pb2 import LiveMeta, StreamMeta, FrameMeta

class Meta(CommandBase):
    def __init__(self, options, *args, **kwargs):
        CommandBase.__init__(self, options, args, kwargs)
        self.face_ = Face()
        self.mustQuit_ = False
        self.try_ = {}
        # check if any of the specific meta flags are present
        # if not -- allow fetch all
        fetchAll = False
        if not (options['--meta'] or
                options['--live'] or
                options['--gop-start'] or
                options['--gop-end'] or
                options['--latest'] or
                options['--frame']):
            fetchAll = True
        if fetchAll:
            self.options['--meta'] = True
            self.options['--live'] = True
            self.options['--gop-start'] = True
            self.options['--gop-end'] = True
            self.options['--latest'] = True
            self.options['--frame'] = True
            self.fetched_ = {
                '_meta' : False, '_latest' : False, '_live' : False,
                '_gop-start': False, '_gop-end': False#, 'frame': False
                }
        else:
            self.fetched_ = {}
            if self.options['--meta']: self.fetched_['_meta'] = False
            if self.options['--live']: self.fetched_['_live'] = False
            if self.options['--gop-start']: self.fetched_['_gop-start'] = False
            if self.options['--gop-end']: self.fetched_['_gop-end'] = False
            if self.options['--latest']: self.fetched_['_latest'] = False
            if self.options['--frame']: self.fetched_['_frame'] = False

    def hasFetchedAll(self):
        fetchedAll = True
        for k,v in self.fetched_.items():
            fetchedAll = fetchedAll & v
        return fetchedAll

    def run(self):
        self.inspectStream(self.options['<stream_prefix>'])
        while not self.hasFetchedAll() and not self.mustQuit_:
            self.face_.processEvents()
            time.sleep(0.01)

    def inspectStream(self, streamPrefix):
        def onStreamMeta(metaNmspc):
            self.fetched_['_meta'] = True
            meta = StreamMeta()
            meta.ParseFromString(metaNmspc.getData().getContent().toBytes())
            self.logMultiline('stream meta:', meta)
            self.logPacketInfo(metaNmspc.getData())

        def onLiveMeta(liveNmspc):
            self.fetched_['_live'] = True
            live = LiveMeta()
            live.ParseFromString(liveNmspc.getData().getContent().toBytes())
            self.logMultiline('live meta', live)
            self.logPacketInfo(liveNmspc.getData())

        def fetchGop(gopPrefix):
            def onGopStart(gopStartNmspc):
                self.fetched_['_gop-start'] = True
                set = DelegationSet()
                set.wireDecode(gopStartNmspc.getData().getContent())
                self.logMultiline('latest gop-start', set.get(0).getName().toUri())
                self.logPacketInfo(gopStartNmspc.getData())

            def onGopEnd(gopEndNmspc):
                self.fetched_['_gop-end'] = True
                set = DelegationSet()
                set.wireDecode(gopEndNmspc.getData().getContent())
                self.logMultiline('latest gop-end', set.get(0).getName().toUri())
                self.logPacketInfo(gopEndNmspc.getData())

            if self.options['--gop-start']:
                self.fetchPacket(Name(gopPrefix).append('start'), onGopStart)
            if self.options['--gop-end']:
                self.fetchPacket(Name(gopPrefix).append('end'), onGopEnd)

        def onLatestPointer(latestNmspc):
            self.fetched_['_latest'] = True
            set = DelegationSet()
            try:
                set.wireDecode(latestNmspc.getData().getContent())
                lines = ['frame: '+set.get(0).getName().toUri()]
                if set.size() == 2:
                    lines.append('gop: '+set.get(1).getName().toUri())
                if set.size() > 2:
                    for i in range(2, set.size()):
                        lines.append('delegation'+str(i)+': '+set.get(i).getName().toUri())
                if self.options['--latest']:
                    self.logMultiline('latest pointer:', *lines)
                    self.logPacketInfo(latestNmspc.getData())
                if set.size() >= 2:
                    fetchGop(set.get(1).getName())
            except:
                self.log('error parsing _latest')


        if self.options['--meta']:
            self.fetchPacket(Name(streamPrefix).append("_meta"), onStreamMeta)
        if self.options['--live']:
            self.fetchPacket(Name(streamPrefix).append("_live"), onLiveMeta, True)
        if (self.options['--latest'] or
           self.options['--gop-start'] or
           self.options['--gop-end'] or
           self.options['--frame']):
            self.fetchPacket(Name(streamPrefix).append("_latest"), onLatestPointer, True)

    def fetchPacket(self, packetPrefix, onFetched, mustBeFresh = False):
        packet = Namespace(packetPrefix)
        packet.setFace(self.face_)
        if not packet.getName().toUri() in self.try_:
            self.try_[packet.getName().toUri()] = 0
        self.timestampRequest(packet)

        def namespaceStateChanged(namespace, changedNamespace, state, clbckId):
            if state == NamespaceState.INTEREST_EXPRESSED:
                self.log('fetching ', namespace.getName())
            elif state == NamespaceState.OBJECT_READY:
                if changedNamespace.getObject():
                    del self.try_[namespace.getName().toUri()]
                    self.timestampReply(namespace, changedNamespace)
                    onFetched(changedNamespace)
                else:
                    self.log('received packet but getObject() is None')
            elif state == NamespaceState.INTEREST_TIMEOUT:
                if self.try_[packet.getName().toUri()] < 3:
                    self.log('timeout fetching', namespace.getName().toUri(),
                             ': retry', self.try_[namespace.getName().toUri()])
                    self.fetchPacket(packetPrefix, onFetched, mustBeFresh)
                else:
                    self.log('timeout fetching', namespace.getName().toUri(),
                             ': retries expired')
            elif state == NamespaceState.INTEREST_NETWORK_NACK or \
                 state == NamespaceState.DECRYPTION_ERROR or \
                 state == NamespaceState.ENCRYPTION_ERROR or \
                 state == NamespaceState.SIGNING_ERROR:
                self.log('unhandled namespace state: ', state, 'for',
                         namespace.getName().toUri())
                self.mustQuit_ = True

        packet.setMaxInterestLifetime(1000)
        packet.addOnStateChanged(namespaceStateChanged)
        packet.objectNeeded(mustBeFresh)

        self.try_[packet.getName().toUri()] += 1
