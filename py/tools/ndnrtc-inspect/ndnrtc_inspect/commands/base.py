"""The base command."""

import textwrap, time, logging, sys
from termcolor import colored
from pyndn import Name, Face, DigestSha256Signature
from pycnl import Namespace, NamespaceState

class CommandBase(object):
    """A base command."""

    def __init__(self, options, *args, **kwargs):
        self.options = options
        self.args = args
        self.kwargs = kwargs
        self.isVerbose_ = (self.options['--v'] or self.options['--vv'])
        self.printOnly_ = False # if True, disables logging, only self.print call is allowed
        self.timestamp_ = {}
        self.face_ = Face()
        self.mustQuit_ = False
        self.try_ = {}
        if self.options['--vv']:
            logging.getLogger('').addHandler(logging.StreamHandler(sys.stdout))
            logging.getLogger('').setLevel(logging.DEBUG)

    def run(self):
        raise NotImplementedError('You must implement the run() method yourself!')

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
                    self.log('timeout. retry '+str(self.try_[namespace.getName().toUri()]),
                             namespace.getName().toUri())
                    self.fetchPacket(packetPrefix, onFetched, mustBeFresh)
                else:
                    self.log('timeout. reties expired', namespace.getName().toUri())
                    self.mustQuit_ = True
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


    def _log(self, *args):
        if self.printOnly_: return
        print(' > ', ' '.join(map(str,args)))

    def log(self, *args):
        highlight = colored(args[0], 'red')
        params = ''
        if len(args) > 1:
            params = ' '.join(map(str,args[1:]))
        self._log(highlight, params)

    def _logIndented(self, *args):
        if self.printOnly_: return
        params = []
        for p in args:
            params.append(p.__str__().rstrip())
        for p in params:
            print(textwrap.indent(p, 8 * ' '))

    def logMultiline(self, caption, *args):
        self._log(colored(caption, 'green'))
        self._logIndented(*args)

    def print(self, *args):
        print(args)

    def logPacketInfo(self, ndnData):
        if self.isVerbose_:
            drd = 'n/a'
            if ndnData.getName().toUri() in self.timestamp_:
                drd = int((time.time() - self.timestamp_[ndnData.getName().toUri()])*1000)
            if type(ndnData.getSignature()) is DigestSha256Signature :
                keyLocator = 'digest: '
                if ndnData.getSignature().getSignature():
                    bytes = ndnData.getSignature().getSignature().toBytes()
                    keyLocator += '0x' + ''.join('{:02x}'.format(x) for x in bytes)
            else:
                keyLocator = ndnData.getSignature().getKeyLocator().getKeyName().toUri()
            self._logIndented(\
                        colored('name:      ', 'blue')+colored(ndnData.getName().toUri(), 'green'),
                        colored('full name: ', 'blue')+colored(ndnData.getFullName().toUri(), 'green'),
                        colored('payload:   ', 'blue')+str(ndnData.getContent().size())+' bytes',
                        colored('wire:      ', 'blue')+str(ndnData.getDefaultWireEncoding().size())+' bytes',
                        colored('signed:    ', 'blue')+keyLocator,
                        colored('freshness: ', 'blue')+str(int(ndnData.getMetaInfo().getFreshnessPeriod())),
                        colored('drd:       ', 'blue')+str(drd)+'ms'
                        )

    def timestampRequest(self, nmspc):
        self.timestamp_[nmspc.getName().toUri()] = time.time()

    def timestampReply(self, nmspc, replyNmspc):
        if not nmspc.getName().equals(replyNmspc.getName()):
            if nmspc.getName().toUri() in self.timestamp_:
                self.timestamp_[replyNmspc.getName().toUri()] = self.timestamp_[nmspc.getName().toUri()]
                del self.timestamp_[nmspc.getName().toUri()]
