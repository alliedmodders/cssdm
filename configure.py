# vim: set ts=2 sw=2 tw=99 noet:
import sys
from ambuild2 import run

# Simple extensions do not need to modify this file.

parser = run.BuildParser(sourcePath=sys.path[0], api='2.2')
parser.options.add_argument('--sm-bin-path', type=str, dest='sm_bin_path', default=None,
							help='Path to SourceMod (binary copy)')
parser.Configure()
