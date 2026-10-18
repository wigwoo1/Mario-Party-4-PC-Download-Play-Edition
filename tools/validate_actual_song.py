"""Private original sequencer validation; no asset payload export."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--native',required=True)
    parser.add_argument('--wasm',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--song',type=int,default=0)
    parser.add_argument('--all',action='store_true')
    args=parser.parse_args()
    disc=Disc(args.disc)
    try:
        asset=next(a for a in inspect(disc)['assets'] if a['path'].endswith('.msm'))
        raw=disc.read(asset['disc_offset'],asset['bytes'])
        song_count=struct.unpack_from('>I',raw,44)[0]//16
        results=[]
        for song_index in range(song_count) if args.all else [args.song]:
            payload=struct.pack('>2I',len(raw),song_index)+raw
            metrics=[]
            for command in ([args.native],['node',args.wasm]):
                result=subprocess.run(command,input=payload,capture_output=True,timeout=60)
                if result.returncode:
                    raise ValueError(f'Song {song_index} reader exit {result.returncode}: {result.stderr.decode(errors="replace")[:500]}')
                metrics.append([int(value) for value in result.stdout.split()])
            if metrics[0]!=metrics[1]:
                raise ValueError('Native/Wasm sequence metrics differ')
            values=metrics[0]
            position=3
            slots=set()
            for _ in range(values[2]):
                if position+4>len(values):
                    raise ValueError('Truncated selected sample header')
                slot, sample_id, offset, count=values[position:position+4]
                position+=4
                if slot in slots or not 0<=slot<64 or not 0<count<=32 or not 0<=sample_id<65535:
                    raise ValueError('Invalid selected sample metadata')
                slots.add(slot)
                pcm=values[position:position+count]
                if len(pcm)!=count or any(not -32768<=sample<=32767 for sample in pcm):
                    raise ValueError('Invalid selected PCM')
                position+=count
            if position!=len(values):
                raise ValueError('Trailing selected sample data')
            results.append({'metrics':values[:3], 'selected_pcm_sha256':hashlib.sha256(json.dumps(values).encode()).hexdigest()})
        with open(args.output,'w',encoding='utf-8') as output:
            json.dump({'scope':'Actual songs, original seqPlaySong and2000seq/synth ticks, per-tick first sample decode per voice slot; no mixed/device audio','metrics':results},output,indent=2)
        print(f'{len(results)} actual original songs native/Wasm PASS')
    finally:
        disc.file.close()
if __name__=='__main__':
    main()
