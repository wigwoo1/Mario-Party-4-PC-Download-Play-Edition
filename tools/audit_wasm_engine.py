"""Compile shared game C sources for Wasm; report platform blockers without stubs."""
import argparse,json,os,re,subprocess,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--work',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sdl-include',type=Path)
a=p.parse_args(); repo=Path(__file__).resolve().parents[1]; sdk=a.sdk.resolve(); work=a.work.resolve()
work.mkdir(parents=True,exist_ok=False)
section=(repo/'files.cmake').read_text().split('set(GAME_FILES',1)[1].split(')',1)[0]
sources=re.findall(r'src/[\w/]+\.c',section)
env=dict(os.environ,EM_CONFIG=str(sdk/'.emscripten'))
flags=['-c','-DTARGET_PC','-DTARGET_DOL','-DNON_MATCHING','-DVERSION=1','-DOPTIMIZED_TEXTURE_LOADING','-DBYTESWAPPING','-DMUSY_TARGET=MUSY_TARGET_PC']
flags+=['-I'+str(repo/d) for d in ['include','extern','extern/aurora/include','extern/aurora/include/dolphin','extern/musyx/include']]
if a.sdl_include: flags+=['-I'+str(a.sdl_include.resolve())]
def compile_source(source):
    target=work/(source.replace('/','_')+'.o')
    result=subprocess.run([sys.executable,str(sdk/'upstream/emscripten/emcc.py'),str(repo/source),*flags,'-o',str(target)],env=env,capture_output=True,text=True,timeout=120)
    log=target.with_suffix('.log'); log.write_text(result.stdout+result.stderr)
    return {'source':source,'compiled':result.returncode==0,'log':str(log),'errors':[l for l in result.stderr.splitlines() if 'error:' in l]}
with ThreadPoolExecutor(max_workers=4) as pool: results=list(pool.map(compile_source,sources))
report={'scope':'Compile-only shared game C sources; no port C++ or Aurora renderer build','version':1,'compiled':sum(r['compiled'] for r in results),'total':len(results),'sources':results}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(f"Compiled {report['compiled']}/{report['total']} shared game C sources")
for r in results:
    if not r['compiled']: print(r['source'],r['errors'][:2])

