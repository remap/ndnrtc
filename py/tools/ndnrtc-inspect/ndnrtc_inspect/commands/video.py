"""video command."""

from .base import CommandBase
from json import dumps

class Video(CommandBase):
    def __init__(self, options, *args, **kwargs):
        CommandBase.__init__(self, options, args, kwargs)

    def run(self):
        print("Hey! Running 'video' command")
        print('You supplied the following options:', dumps(self.options, indent=2, sort_keys=True))
