"""Private descriptor-to-original MIDI parameter comparison; save counts/hash only."""
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
    args=parser.parse_args()
    disc=Disc(args.disc)
    results=[]
    try:
        for asset in inspect(disc)['assets']:
            if not asset['path'].endswith('.msm'): continue
            raw=disc.read(asset['disc_offset'],asset['bytes'])
            offset,length=struct.unpack_from('>II',raw,48)
            if length%16: raise ValueError('Partial effect table')
            expected=[]
            accepted=0
            for index in range(length//16):
                group,effect,volume,pan,pitch,span,reverb,chorus,emitter,component=struct.unpack_from('>HHbbhBBBbb',raw,offset+16*index)
                valid=group!=65535 and 0<=volume<=127 and 0<=pan<=127 and reverb<=127 and chorus<=127
                expected.append(int(valid))
                if valid:
                    accepted+=1
                    expected.extend((7,volume*129,10,pan*129,91,reverb,93,chorus,128,max(0,min(16383,pitch+8192))))
            payload=struct.pack('>I',len(raw))+raw
            outputs=[]
            for command in ([args.native],['node',args.wasm]):
                result=subprocess.run(command,input=payload,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60,check=True)
                values=list(map(int,result.stdout.split()))
                if values!=expected: raise ValueError('Descriptor parameter reference mismatch')
                outputs.append(' '.join(map(str,values)).encode('ascii'))
            if outputs[0]!=outputs[1]: raise ValueError('Native/Wasm mismatch')
            results.append({'effects':length//16,'accepted':accepted,'parameter_sha256':hashlib.sha256(outputs[0]).hexdigest()})
    finally:
        disc.file.close()
    with open(args.output,'w',encoding='utf-8') as target:
        json.dump({'scope':'All MSM default non-positional parameter calculations, independent Python/native/Wasm; no effect dispatch or audio output','results':results},target,indent=2)
    print('Actual effect parameter reference PASS',results)

if __name__=='__main__': main()
