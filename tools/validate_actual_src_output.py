"""Compare private authored dry effects through the full software audio chain."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--native',required=True)
    parser.add_argument('--wasm',required=True)
    parser.add_argument('--effects',type=int,nargs='+',default=[0,50,400,800,1600,2200])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    disc=Disc(args.disc)
    try:
        asset=next(a for a in inspect(disc)['assets'] if a['path'].endswith('.msm'))
        raw=disc.read(asset['disc_offset'],asset['bytes'])
        count=struct.unpack_from('>I',raw,52)[0]//16
        assert all(0<=i<count for i in args.effects) and len(set(args.effects))==len(args.effects)
        results=[]
        for index in args.effects:
            payload=struct.pack('>II',len(raw),index)+raw
            pair=[]
            for command in ([args.native],['node',args.wasm]):
                run=subprocess.run(command,input=payload,capture_output=True,timeout=60)
                if run.returncode:
                    raise ValueError(f'{command[0]} effect{index}: exit{run.returncode}: {run.stderr.decode(errors="replace")[:1200]}')
                assert not run.stderr,run.stderr.decode(errors='replace')
                result=json.loads(run.stdout)
                assert result['effect']==index and result['frames']==32000 and result['blocks']==200
                assert result['nonzeroSamples']>100 and result['peak']>10 and result['controlPeriods']==1000
                pair.append(result)
            assert pair[0]==pair[1],f'Native/Wasm effect{index} PCM/counters differ'
            results.append(pair[0])
        report=dict(status='VERIFIED scoped original-effect dry software pipeline, native/Wasm',
                    coefficientProvider='partyboard-windowed-sinc-q15-v1',
                    scope='One second per effect; original SDK macro/control scheduling, registered samples, replacement SRC, nine-bus/studio mix and real SDL standalone stream; exact native/Wasm hashes and counters',
                    effects=results,assetPath=asset['path'],assetBytes=len(raw),assetSha256=hashlib.sha256(raw).hexdigest(),
                    sourceFrames=sum(r['frames'] for r in results),
                    productionGameAdapterIntegrated=False,fullGameAudioVerified=False,authoredAuxEnabled=False,
                    audioDeviceOpened=False,privatePayloadWritten=False)
        args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(report))
    finally:
        disc.file.close()


if __name__=='__main__':
    main()
