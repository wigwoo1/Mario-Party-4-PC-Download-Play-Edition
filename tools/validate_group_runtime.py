"""Compare actual group startup/first-effect fixtures; export metrics only."""
import argparse
import hashlib
import json
import struct
import subprocess
from inspect_disc import Disc
from inspect_audio_assets import inspect

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('disc');p.add_argument('--native',required=True)
p.add_argument('--wasm',required=True);p.add_argument('--output',required=True)
a=p.parse_args();d=Disc(a.disc)
try:
    asset=next(x for x in inspect(d)['assets'] if x['path'].endswith('.msm'))
    raw=d.read(asset['disc_offset'],asset['bytes'])
    words=struct.unpack_from('>24I',raw)
    info=raw[words[4]:words[4]+64]
    bases=list(info[41:41+info[40]])
    descriptors=[struct.unpack_from('>Hbb4I',raw,words[8]+i*32) for i in range(words[9]//32)]
    results=[]
    for index,descriptor in enumerate(descriptors):
        if not descriptor[4]: continue
        indices=bases+[index] if index not in bases else list(bases)
        payload=struct.pack('>2I',len(indices),indices.index(index))
        for group in indices:
            _,_,_,offset,length,sample_offset,sample_bytes=descriptors[group]
            if not length or not sample_bytes: raise ValueError('Empty registered group')
            payload+=struct.pack('>2I',length,sample_bytes)
            payload+=raw[words[14]+offset:words[14]+offset+length]
            payload+=raw[words[16]+sample_offset:words[16]+sample_offset+sample_bytes]
        outputs=[]
        for command in ([a.native],['node',a.wasm]):
            result=subprocess.run(command,input=payload,capture_output=True,timeout=60)
            if result.returncode: raise ValueError(f'Group {index} exit {result.returncode}')
            outputs.append(result.stdout)
        if not outputs[0] or outputs[0]!=outputs[1]:
            first=next((i for i,(x,y) in enumerate(zip(*outputs)) if x!=y),None)
            raise ValueError(f'Group {index} output mismatch: lengths {[len(x) for x in outputs]}, first byte {first}, headers {[x[:12].hex() for x in outputs]}')
        output=outputs[0]
        if len(output)<12: raise ValueError('Truncated group result')
        loaded,started,selected=struct.unpack_from('>3I',output)
        group_base=words[14]+descriptor[3]
        project=group_base+struct.unpack_from('>I',raw,group_base+4)[0]
        project_type=struct.unpack_from('>H',raw,project+6)[0]
        expected_start=0
        if project_type==1:
            table=project+struct.unpack_from('>I',raw,project+28)[0]
            expected_start=int(struct.unpack_from('>H',raw,table)[0]>0)
        if loaded!=len(indices) or started!=expected_start or selected>64:
            raise ValueError('Group runtime header contradicts descriptor')
        # Successful dispatch does not prove source selection in this short
        # 20-tick window. Keep zero selections explicit in the evidence.
        cursor=12;slots=set()
        for _ in range(selected):
            if cursor+16>len(output): raise ValueError('Truncated sample header')
            slot,sample_id,offset,count=struct.unpack_from('>4I',output,cursor)
            cursor+=16
            if slot>=64 or slot in slots or sample_id>=65535 or not 0<count<=32:
                raise ValueError('Invalid selected sample result')
            slots.add(slot);cursor+=count*2
            if cursor>len(output): raise ValueError('Truncated sample excerpt')
        if cursor!=len(output): raise ValueError('Trailing group result')
        results.append({'group':index,'binary_metrics_bytes':len(outputs[0]),
            'loaded_groups':loaded,'effect_started':started,'selected_slots':selected,
            'metrics_sha256':hashlib.sha256(outputs[0]).hexdigest()})
    json.dump({'scope':'Actual base/target group ownership, first effect/20 ticks and selected sample/envelope fixtures; no mixed/device audio',
        'base_groups':bases,'groups':results},open(a.output,'w'),indent=2)
    print(f'{len(results)} actual group runtime native/Wasm comparisons PASS')
finally:
    d.file.close()
