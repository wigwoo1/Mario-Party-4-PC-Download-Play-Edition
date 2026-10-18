"""Private original effect dispatch validation; no asset payload export."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

def parse_source_modes(stderr,prefix='SRC_MODES '):
    lines=[line.split()[1:] for line in stderr.decode(errors='replace').splitlines()
        if line.startswith(prefix)]
    if len(lines)>1:
        raise ValueError('Duplicate source-mode metadata')
    if not lines:
        return None
    modes=[int(value) for value in lines[0]]
    if len(modes)!=3 or any(value<0 for value in modes):
        raise ValueError('Invalid source-mode coverage')
    return modes

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('disc')
    parser.add_argument('--native',required=True)
    parser.add_argument('--wasm',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--effect',type=int,default=0)
    parser.add_argument('--effects',type=int,nargs='+',help='Validate selected descriptor indices in one report')
    parser.add_argument('--replacement-src',action='store_true',help='Require explicit open-source SRC provider metadata from matching render fixtures')
    parser.add_argument('--all',action='store_true')
    parser.add_argument('--positional',action='store_true')
    parser.add_argument('--oneshot',action='store_true')
    parser.add_argument('--voice-render-diagnostic',action='store_true',help='Synthetic-bank decoded voice/envelope/gain pipeline fixture')
    parser.add_argument('--voice-block-render-diagnostic',action='store_true',help='Synthetic-bank original mixer startup and remaining five-period block fixture')
    parser.add_argument('--voice-bus-render-diagnostic',action='store_true',help='Compare all nine numerical channels of block fixture')
    parser.add_argument('--voice-planned-render-diagnostic',action='store_true',help='Separate host scheduling and numerical rendering; use matching planned binaries')
    parser.add_argument('--continuous',action='store_true',help='Select actual valid continuous-emitter descriptors')
    args=parser.parse_args()
    if args.effects is not None and (args.all or not args.effects or len(set(args.effects))!=len(args.effects)):
        parser.error('--effects requires unique indices and cannot be combined with --all')
    if args.voice_planned_render_diagnostic:
        args.voice_bus_render_diagnostic=True
    if args.voice_bus_render_diagnostic:
        args.voice_block_render_diagnostic=True
    if args.voice_block_render_diagnostic:
        args.voice_render_diagnostic=True
    if args.voice_render_diagnostic and args.positional:
        parser.error('--voice-render-diagnostic uses the non-positional rendering fixture')
    if args.replacement_src and not args.voice_render_diagnostic:
        parser.error('--replacement-src requires a voice-render fixture')
    if args.continuous and not (args.positional and args.all):
        parser.error('--continuous requires --positional --all')
    if args.oneshot and (not args.positional or args.continuous):
        parser.error('--oneshot requires --positional without --continuous')
    disc=Disc(args.disc)
    try:
        asset=next(a for a in inspect(disc)['assets'] if a['path'].endswith('.msm'))
        raw=disc.read(asset['disc_offset'],asset['bytes'])
        effect_count=struct.unpack_from('>I',raw,52)[0]//16
        results=[]
        indices=range(effect_count) if args.all else args.effects if args.effects is not None else [args.effect]
        if any(index<0 or index>=effect_count for index in indices):
            parser.error('Effect descriptor index is outside the authored table')
        for effect_index in indices:
            descriptor_offset=struct.unpack_from('>I',raw,48)[0]+effect_index*16
            if args.oneshot and args.all:
                group,fxid=struct.unpack_from('>HH',raw,descriptor_offset)
                # Tombstones have no playable voice or auxiliary controllers.
                # Rejection is covered by the complete non-positional corpus.
                if group==65535 or fxid==65535:
                    continue
            if args.continuous:
                group,fxid=struct.unpack_from('>HH',raw,descriptor_offset)
                emitter=struct.unpack_from('>b',raw,descriptor_offset+11)[0]
                if emitter!=1 or group==65535 or fxid==65535:
                    continue
            payload=struct.pack('>2I',len(raw),effect_index)+raw
            metrics=[]
            source_modes=[]
            selected_banks=[]
            for command in ([args.native],['node',args.wasm]):
                result=subprocess.run(command,input=payload,capture_output=True,timeout=60)
                if result.returncode:
                    raise ValueError(f'Effect {effect_index} reader exit {result.returncode}: {result.stderr.decode(errors="replace")[:500]}')
                metrics.append([int(value) for value in result.stdout.split()])
                providers=[line.split()[1:] for line in result.stderr.decode(errors='replace').splitlines() if line.startswith('SRC_PROVIDER ')]
                if args.replacement_src:
                    if providers!=[['partyboard-windowed-sinc-q15-v1']]:
                        raise ValueError('Missing or incorrect replacement SRC provider')
                elif providers:
                    raise ValueError('Replacement SRC binary requires --replacement-src; refusing synthetic-bank label')
                source_modes.append(parse_source_modes(result.stderr))
                selected_banks.append(parse_source_modes(result.stderr,'SRC_BANKS '))
            if metrics[0]!=metrics[1]:
                raise ValueError('Native/Wasm sequence metrics differ')
            values=metrics[0]
            if len(values)<3 or values[0]!=effect_index or not 0<values[1]<=24 or not 0<=values[2]<=64:
                raise ValueError('Invalid effect result header')
            if source_modes[0]!=source_modes[1]:
                raise ValueError('Native/Wasm source-mode coverage differs')
            if selected_banks[0]!=selected_banks[1] or (args.replacement_src and
                    (selected_banks[0] is None or sum(selected_banks[0])!=values[2])):
                raise ValueError('Invalid or mismatched native/Wasm coefficient bank coverage')
            if source_modes[0] is not None and (len(source_modes[0])!=3
                    or any(value<0 for value in source_modes[0]) or sum(source_modes[0])!=values[2]):
                raise ValueError('Invalid source-mode coverage')
            position=3
            slots=set()
            selected_frame_counts=[]
            nonzero_pcm_values=0
            for _ in range(values[2]):
                if position+4>len(values):
                    raise ValueError('Truncated selected sample header')
                slot, sample_id, offset, count=values[position:position+4]
                position+=4
                max_frames=160 if args.voice_block_render_diagnostic else 32
                if args.voice_bus_render_diagnostic:
                    max_frames*=9
                    if count%9:
                        raise ValueError('Incomplete numerical mixer frame')
                if slot in slots or not 0<=slot<64 or not 0<count<=max_frames or not 0<=sample_id<65535:
                    raise ValueError('Invalid selected sample metadata')
                slots.add(slot)
                selected_frame_counts.append(count//9 if args.voice_bus_render_diagnostic else count)
                pcm=values[position:position+count]
                if len(pcm)!=count or any(not -32768<=sample<=32767 for sample in pcm):
                    raise ValueError('Invalid selected PCM')
                nonzero_pcm_values+=sum(value!=0 for value in pcm)
                position+=count
            if position!=len(values):
                raise ValueError('Trailing selected sample data')
            effect_offset=struct.unpack_from('>I',raw,48)[0]+effect_index*16
            invalid_fx=struct.unpack_from('>H',raw,effect_offset+2)[0]==65535
            zero_volume_culled=args.oneshot and raw[effect_offset+4]==0
            if zero_volume_culled and values[2]!=0:
                raise ValueError('Silent positional effect produced sample voices')
            if invalid_fx and values[2]!=0:
                raise ValueError('Invalid FXID produced sample voices')
            results.append({'metrics':values[:3], 'invalid_fxid_rejected':invalid_fx,
                'zero_volume_culled':zero_volume_culled,
                'selected_frame_counts':selected_frame_counts,
                'selected_nonzero_pcm_values':nonzero_pcm_values,
                'selected_source_mode_counts':source_modes[0],
                'selected_coefficient_bank_counts':selected_banks[0],
                'selected_pcm_sha256':hashlib.sha256(json.dumps(values).encode()).hexdigest()})
            if len(results)%100==0:
                print(f'{len(results)} effects compared',flush=True)
        with open(args.output,'w',encoding='utf-8') as output:
            scope=('Actual one-shot positional effects, original listener/NULL emitter API; immediate reverb23/chorus45 controller assertions'
                if args.oneshot else 'Actual continuous positional effects, original listener/emitter and s3dHandle every five synth ticks'
                if args.positional else 'Actual default non-positional effects, original sndFXStartParaInfo')
            if args.voice_render_diagnostic:
                scope='Actual original effect-selected samples through owned decoder/SRC/envelope/PB-gain pipeline; synthetic newest-tap bank, no original ROM fidelity; first rendered dry-left excerpt per slot'
            detail=(';2000seq/synth ticks, first selected voice period rendered into numerical buses; no device audio'
                if args.voice_render_diagnostic else ';2000seq/synth ticks for started voices, zero-volume one-shots culled before ticks; per-tick first sample decode per voice slot; no mixed/device audio')
            if args.voice_block_render_diagnostic:
                detail=';2000 seq/synth ticks, original standard mixer startup and remaining periods of first selected voice block rendered; all rendered dry-left frames compared, at most160 per slot; no device audio'
            if args.voice_bus_render_diagnostic:
                scope='Actual original effect-selected samples through owned decoder/SRC/envelope/PB-gain pipeline; synthetic newest-tap bank, no original ROM fidelity'
                detail=';2000 seq/synth ticks, original standard mixer startup and remaining first selected block periods rendered; all nine clipped numerical bus channels compared for up to160 frames per slot; no studio effects or device audio'
            if args.voice_planned_render_diagnostic:
                detail+='; separate original host block plan and recorded-period renderer, first block only; continuation completion boundary is not exercised; no production audio or DSP tail acceptance'
            if args.replacement_src:
                scope=scope.replace('synthetic newest-tap bank, no original ROM fidelity',
                    'approved Partyboard open-source windowed-sinc replacement, not original console coefficients')
            json.dump({'scope':scope+detail,
                'coefficient_provider':'partyboard-windowed-sinc-q15-v1' if args.replacement_src else None,
                'source_effect_count':effect_count,'compared_effect_count':len(results),
                'metrics':results},output,indent=2)
        print(f'{len(results)} actual original effects native/Wasm PASS')
    finally:
        disc.file.close()
if __name__=='__main__':
    main()
