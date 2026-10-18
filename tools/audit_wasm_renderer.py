"""Compile Aurora GX sources against browser bindings, without substituting engine stubs."""
import argparse,json,os,re,subprocess,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--deps',type=Path,required=True)
p.add_argument('--work',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args(); repo=Path(__file__).resolve().parents[1]; aurora=repo/'extern/aurora'
sdk=a.sdk.resolve(); deps=a.deps.resolve(); work=a.work.resolve(); work.mkdir(parents=True,exist_ok=False)
section=(aurora/'cmake/aurora_gx.cmake').read_text().split('add_library(aurora_gx STATIC',1)[1].split(')',1)[0]
sources=re.findall(r'lib/[\w/]+\.cpp',section)
env=dict(os.environ,EM_CONFIG=str(sdk/'.emscripten'))
flags=['-c','--use-port=emdawnwebgpu','-sUSE_SDL=3','-std=c++20','-DAURORA','-DTARGET_PC','-DAURORA_ENABLE_GX']
includes=[aurora/'include',deps/'fmt-src/include',deps/'abseil-cpp-src',deps/'xxhash-src',deps/'tracy-src/public',deps/'sqlite3-src',deps/'png-src',deps/'png-build',deps/'zlib-src',deps/'zlib-build']
flags+=['-I'+str(d) for d in includes]
def compile_source(source):
    target=work/(source.replace('/','_')+'.o')
    r=subprocess.run([sys.executable,str(sdk/'upstream/emscripten/emcc.py'),str(aurora/source),*flags,'-o',str(target)],env=env,capture_output=True,text=True,timeout=180)
    log=target.with_suffix('.log'); log.write_text(r.stdout+r.stderr)
    return {'source':source,'compiled':r.returncode==0,'log':str(log),'errors':[l for l in r.stderr.splitlines() if 'error:' in l]}
with ThreadPoolExecutor(max_workers=4) as pool: results=list(pool.map(compile_source,sources))
report={'scope':'Browser compile-only Aurora GX audit; not linked or executed; native dependency headers reused','compiled':sum(r['compiled'] for r in results),'total':len(results),'sources':results}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(f"Compiled {report['compiled']}/{report['total']} Aurora GX sources")
for r in results:
    if not r['compiled']: print(r['source'],r['errors'][:2])
