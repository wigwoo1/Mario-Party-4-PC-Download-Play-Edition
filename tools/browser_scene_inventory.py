"""Inventory observed USA overlays; never certify shader completeness."""
import argparse, hashlib, json, re
from pathlib import Path

def inventory(table, provenance):
    table=Path(table); names=[]; active=True; conditional=False
    for line in table.read_text().splitlines():
        line=line.strip()
        if line=='#if VERSION_JP':
            if conditional: raise ValueError('Nested overlay conditional')
            conditional=True;active=False
        elif line=='#else':
            if not conditional: raise ValueError('Unexpected overlay else')
            active=True
        elif line=='#endif':
            if not conditional: raise ValueError('Unexpected overlay endif')
            conditional=False;active=True
        elif line.startswith('#if'): raise ValueError('Unsupported overlay conditional')
        elif active:
            match=re.fullmatch(r'DLL\((\w+)\)',line)
            if match: names.append(match[1])
    if conditional or len(names)!=len(set(names)): raise ValueError('Invalid overlay table')
    observed={};sources=[]
    for source in provenance:
        source=Path(source);data=json.loads(source.read_text());cache=Path(str(source)[:-5])
        if data.get('scope')!='observed-scenes-only' or data.get('fullCoverage') is not False:
            raise ValueError('Explicit observed-only provenance required')
        raw=cache.read_bytes()
        if len(raw)!=data['bytes'] or hashlib.sha256(raw).hexdigest()!=data['sha256']:
            raise ValueError('Provenance cache identity differs')
        visits=data['simulationSceneVisits']
        if not visits: raise ValueError('Scene observer evidence missing')
        for key,visit in visits.items():
            overlay=visit['overlay']
            if type(overlay)!=int or str(overlay)!=key or not 0<=overlay<len(names) or type(visit['samples'])!=int or visit['samples']<1:
                raise ValueError('Invalid observed overlay')
            if not 0<=visit['firstFrame']<=visit['lastFrame']: raise ValueError('Invalid observed frames')
            observed.setdefault(overlay,[]).append({'provenance':str(source),'samples':visit['samples']})
        selected=data.get('selectedMinigame')
        if data['endpoint'] in ('minigame','results','board-return') and (not selected or str(selected['overlay']) not in visits):
            raise ValueError('Selected minigame execution missing')
        sources.append(str(source))
    rows=[{'overlay':i,'name':name,'simulationObserved':i in observed,'evidence':observed.get(i,[]),'shaderCoverageVerified':False} for i,name in enumerate(names)]
    return {'version':1,'region':'USA','scope':'simulation-observations-only','fullCoverage':False,
            'tableSha256':hashlib.sha256(table.read_bytes()).hexdigest(),'sources':sources,
            'overlayCount':len(rows),'observedOverlayCount':len(observed),'overlays':rows}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--table',required=True);parser.add_argument('--output',required=True);parser.add_argument('provenance',nargs='+');args=parser.parse_args()
    result=inventory(args.table,args.provenance);Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:result[key] for key in ['overlayCount','observedOverlayCount','fullCoverage']}))
