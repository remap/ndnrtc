"""
ndnrtc-inspect

Usage:
  ndnrtc-inspect meta <stream_prefix> [--meta] [--live] [--gop-start] [--gop-end] [--latest] [--frame] [--ds=<delegation_no>] [(--v | --vv)]
  ndnrtc-inspect video <stream_prefix> --out=<encoded_video_file> [--pipeline=<pipeline>] [--duration=<frames>] [--start=<frame#>] [(--v | --vv)]
  ndnrtc-inspect frame (<frame_prefix> | - ) [--meta] [--manifest] [--segments] [--parity] [(--v | --vv)]
  ndnrtc-inspect -h | --help
  ndnrtc-inspect --version

Arguments:
  meta          Fetch stream metadata. If none of the specific metadata flags
                are present (like --live, --gop-start, etc.), will fetch all,
                except latest frame metadata (speciy --frame for that).
                metadata; otherwise -- will fetch only those specified.
  video         Fetches video payload according to the flags specified.
  frame         Fetches data and metadata of an NDN-RTC stream.

Options:
  --meta                            Fetch stream _meta packet only
  --gop-start                       Fetch latest gop start packet only
  --gop-end                         Fetch latest gop end packet only
  --live                            Fetch _live packet only
  --latest                          Fetch _latest packet only
  --frame                           Fetch latest frame only
  -o,--out=<file_pipe>              Output file (or file pipe) for received encoded video frames.
                                    Use ndnrtc-codec tool to decode it.
  --ds=<delegation_no>              If this flag is present, app will output only delegation
                                    under specified index from the delegation set packet. Affects
                                    only "delegation set" packets (e.g. _latest, _gop/start, etc.).
  -d,--duration=<frames>            Number of frames to fetch. If 0 -- fetches continuously until
                                    interrupted [default: 0]
  -s,--start=<frame#>               Frame numbber to start fetching from. If omitted, fetches
                                    from the latest available.
  -p,--pipeline=<pipeline_size>     Pipeline size for fetching frames [default: 15]
  -h --help                         Show this screen.
  --version                         Show version.
  --v                               Verbose level: debug
  --vv                              Verbose level: trace

Examples:
  ndnrtc-inspect meta /ndnrtc/my-stream
  ndnrtc-inspect meta /ndnrtc/my-stream --gop-start --gop-end
  ndnrtc-inspect meta /ndnrtc/my-stream --latest --ds=0
  ndnrtc-inspect video /ndnrtc/my-stream --out=video.pipe
  ndnrtc-inspect video /ndnrtc/my-stream --out=video.pipe -s 460 -d 360
  ndnrtc-inspect frame /ndnrtc/my-stream/frame --meta
  ndnrtc-inspect frame /ndnrtc/my-stream/frame --segments
  ndnrtc-inspect frame /ndnrtc/my-stream/frame > encoded.frame
  ndnrtc-inspect meta /ndnrtc/my-stream --latest --ds=0 | ndnrtc-inspect frame -
  ndnrtc-inspect meta /ndnrtc/my-stream --gop-end --ds=0 | ndnrtc-inspect frame -

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
