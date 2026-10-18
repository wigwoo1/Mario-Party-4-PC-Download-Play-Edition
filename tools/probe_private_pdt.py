"""Pass actual private PDT to a local playback probe without extracting it."""
import argparse
import subprocess
from inspect_audio_assets import inspect
from inspect_disc import Disc

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('disc')
parser.add_argument('probe', nargs='+')
parser.add_argument('--private-api', action='store_true')
args = parser.parse_args()
disc = Disc(args.disc)
try:
    asset = next(asset for asset in inspect(disc)['assets'] if asset['path'].endswith('.pdt'))
    command = args.probe + (['--private-pdt'] if args.private_api else [])
    subprocess.run(command, input=disc.read(asset['disc_offset'], asset['bytes']), check=True)
finally:
    disc.file.close()
