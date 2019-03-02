"""
ndnrtc-inspect

Usage:
  ndnrtc-inspect meta <stream_prefix> [--verbose] [--meta] [--live] [--gop-start] [--gop-end] [--latest] [--frame]
  ndnrtc-inspect video <stream_prefix> --out=<encoded_video_pipe> [--duration=<frames>] [--start=<frame#>] [--verbose]
  ndnrtc-inspect -h | --help
  ndnrtc-inspect --version

Arguments:
  meta          Fetch stream metadata. If none of the specific metadata flags
                are present (like --live, --gop-start, etc.), will fetch all
                metadata; otherwise -- will fetch only those specified.
  video         Fetches video payload according to the flags specified.

Options:
  -h --help                         Show this screen.
  --version                         Show version.
  -o,--out=<file_pipe>              Output file (or file pipe) for received encoded video frames.
                                    Use ndnrtc-codec tool to decode it.
  -d,--duration=<frames>            Number of frames to fetch. If 0 -- fetches continuously until
                                    interrupted [default: 0]
  -s,--start=<frame#>               Frame numbber to start fetching from. If omitted, fetches
                                    from the latest available.
  -v --verbose                      Verbose output
  --meta                            Fetch stream _meta packet only
  --gop-start                       Fetch latest gop start packet only
  --gop-end                         Fetch latest gop end packet only
  --live                            Fetch _live packet only
  --latest                          Fetch _latest packet only
  --frame                           Fetch latest frame only

Examples:
  ndnrtc-inspect meta /ndnrtc/my-stream
  ndnrtc-inspect video /ndnrtc/my-stream --out=video.pipe
  ndnrtc-inspect video /ndnrtc/my-stream --out=video.pipe -s 460 -d 360

Help:
  For help using this tool, please open an issue on the Github repository:
  https://github.com/remap/ndnrtc
"""

from inspect import getmembers, isclass
from docopt import docopt
from . import __version__ as VERSION

def main():
    """Main CLI entrypoint."""
    import ndnrtc_inspect.commands
    options = docopt(__doc__, version=VERSION)

    for (k, v) in options.items():
        if hasattr(ndnrtc_inspect.commands, k) and v:
            module = getattr(ndnrtc_inspect.commands, k)
            commands = getmembers(module, isclass)
            command = [(name,cls) for (name,cls) in commands if name.lower() == k][0][1]
            ndnrtc_inspect.commands = commands
            command = command(options)
            command.run()
