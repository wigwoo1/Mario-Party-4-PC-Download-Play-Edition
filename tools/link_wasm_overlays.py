"""Link complete selected overlays as Wasm side modules; inspect, do not execute."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys
from browser_module_manifest import inventory

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk',type=Path,required=True)
parser.add_argument('--work',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--module',action='append',required=True)
parser.add_argument('--asyncify',action='store_true',help='Instrument overlay imports so Asyncify can suspend and resume across side modules')
parser.add_argument('--entry-only',action='store_true',help='Export only ObjectSetup, keeping overlay data private')
parser.add_argument('--debug-names',action='store_true',help='Retain function names for mapping actual Wasm crash frames')
parser.add_argument('--ment-input-diagnostics',action='store_true',help='Log Party setup callback activation and consumed input in test builds')
parser.add_argument('--minigame-input-diagnostics',action='store_true',help='Observe Tree Stomp human input and position in browser test builds')
parser.add_argument('--instruction-input-diagnostics',action='store_true',help='Observe instruction readiness and accepted human buttons in test builds')
parser.add_argument('--jobs',type=int,default=1,help='Number of overlay modules to compile concurrently')
args=parser.parse_args()
if args.jobs < 1: parser.error('--jobs must be at least 1')
repo=Path(__file__).resolve().parents[1]
# Preserve mapped drive paths on Windows.  Resolving W:\work\emsdk here can
# turn it into a long C: path and make Emscripten treat the existing SDK cache
# as a different configuration (then attempt to erase/rebuild the cache).
sdk=args.sdk.absolute()
work=args.work.resolve()
work.mkdir(parents=True,exist_ok=False)
modules={m['name'].lower():m for m in inventory(repo)['modules']}
env=dict(os.environ,EM_CONFIG=str(sdk/'.emscripten'))
env['PATH']=str(Path(sys.executable).parent)+os.pathsep+env['PATH']
flags=['-DTARGET_PC','-DNON_MATCHING','-DVERSION=1','-DOPTIMIZED_TEXTURE_LOADING',
       '-DBYTESWAPPING','-DMUSY_TARGET=MUSY_TARGET_PC','-sSIDE_MODULE=1','-Wl,-Bsymbolic',
       '-Wl,--fatal-warnings']
if args.ment_input_diagnostics:
    flags += ['-DPARTYBOARD_MENT_INPUT_DIAGNOSTICS']
if args.minigame_input_diagnostics:
    flags += ['-DPARTYBOARD_MINIGAME_INPUT_DIAGNOSTICS']
if args.instruction_input_diagnostics:
    flags += ['-DPARTYBOARD_INSTRUCTION_INPUT_DIAGNOSTICS']
flags+=['-I'+str(repo/p) for p in ['include','extern','extern/aurora/include',
                                  'extern/aurora/include/dolphin','extern/musyx/include']]
if args.debug_names:
    flags += ['--profiling-funcs']
if args.asyncify:
    flags += ['-sASYNCIFY', '-sASYNCIFY_IMPORTS=*']
if args.entry_only:
    flags.remove('-sSIDE_MODULE=1')
    flags += ['-sSIDE_MODULE=2', '-Wl,--export=ObjectSetup']
reports=[]
def link_one(name):
    module=modules[name.lower()]
    if not module['sources']: raise ValueError('No listed sources for '+name)
    binary=work/(module['name']+'.wasm')
    command=[sys.executable,str(sdk/'upstream/emscripten/emcc.py'),
             *[str(repo/s) for s in module['sources']],*flags,'-o',str(binary)]
    result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=180)
    (work/(module['name']+'.log')).write_text(result.stdout+result.stderr)
    report={'name':module['name'],'sources':module['sources'],'linked':result.returncode==0,'asyncify':args.asyncify,'entry_only':args.entry_only,'ment_input_diagnostics':args.ment_input_diagnostics}
    if result.returncode==0:
        inspection=subprocess.run(['node','-e',
          "const fs=require('fs'); const m=new WebAssembly.Module(fs.readFileSync(process.argv[1])); console.log(JSON.stringify({imports:WebAssembly.Module.imports(m),exports:WebAssembly.Module.exports(m)}));",str(binary)],
          check=True,capture_output=True,text=True,timeout=30)
        report.update(json.loads(inspection.stdout))
        report['bytes']=binary.stat().st_size
        report['has_entry']=any(e['name']=='ObjectSetup' and e['kind']=='function' for e in report['exports'])
        assert report['has_entry'], 'Missing ObjectSetup: '+name
        if args.asyncify:
            shared = {e['name'] for e in report['imports'] if e['kind'] == 'global'}
            controls = {e['name'] for e in report['exports'] if e['kind'] == 'function'}
            assert {'__asyncify_state', '__asyncify_data'} <= shared, 'Missing shared Asyncify state: '+name
            assert {'asyncify_start_unwind', 'asyncify_stop_unwind',
                    'asyncify_start_rewind', 'asyncify_stop_rewind',
                    'asyncify_get_state'} <= controls, 'Missing Asyncify resume controls: '+name
            report['asyncify_instrumentation_verified'] = True
        print(f"{name}: linked {report['bytes']} bytes, {len(report['imports'])} imports; ObjectSetup exported")
    report['minigame_input_diagnostics']=args.minigame_input_diagnostics
    report['instruction_input_diagnostics']=args.instruction_input_diagnostics
    return report
with ThreadPoolExecutor(max_workers=args.jobs) as executor:
    reports=list(executor.map(link_one,args.module))
args.output.write_text(json.dumps({'scope':'Link and WebAssembly validation only; no instantiation or execution',
    'artifacts':str(work),'modules':reports},indent=2)+'\n')
if not all(r['linked'] for r in reports): sys.exit(2)

