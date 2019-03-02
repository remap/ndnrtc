"""The base command."""

from termcolor import colored
import textwrap
import time

class CommandBase(object):
    """A base command."""

    def __init__(self, options, *args, **kwargs):
        self.options = options
        self.args = args
        self.kwargs = kwargs
        self.isVerbose_ = self.options['--verbose']
        self.timestamp_ = {}

    def run(self):
        raise NotImplementedError('You must implement the run() method yourself!')

    def _log(self, *args):
        print(' > ', ' '.join(map(str,args)))

    def log(self, *args):
        highlight = colored(args[0], 'red')
        params = ''
        if len(args) > 1:
            params = ' '.join(map(str,args[1:]))
        self._log(highlight, params)

    def _logIndented(self, *args):
        params = []
        for p in args:
            params.append(p.__str__().rstrip())
        for p in params:
            print(textwrap.indent(p, 8 * ' '))

    def logMultiline(self, caption, *args):
        self._log(colored(caption, 'green'))
        self._logIndented(*args)

    def logPacketInfo(self, ndnData):
        if self.isVerbose_:
            drd = 'n/a'
            if ndnData.getName().toUri() in self.timestamp_:
                drd = int((time.time() - self.timestamp_[ndnData.getName().toUri()])*1000)
            self._logIndented(\
                        colored('name:      ', 'blue')+colored(ndnData.getName().toUri(), 'green'),
                        colored('payload:   ', 'blue')+str(ndnData.getContent().size())+' bytes',
                        colored('wire:      ', 'blue')+str(ndnData.getDefaultWireEncoding().size())+' bytes',
                        colored('signed:    ', 'blue')+str(ndnData.getSignature().getKeyLocator().getKeyName()),
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
