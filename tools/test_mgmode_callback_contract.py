"""Exercise the actual menu callback adapters through strict Wasm indirect calls."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--cache', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
parser.add_argument('--node', default='node')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
source = (repo / 'src/REL/mgmodedll/main.c').read_text()
header = (repo / 'include/REL/mgmodedll.h').read_text()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=False)
(work / 'temp').mkdir()
sdk = args.sdk.absolute()
environment = dict(os.environ, EM_CONFIG=str(sdk / '.emscripten'),
                   EM_CACHE=str(args.cache.resolve()), EM_FROZEN_CACHE='1',
                   EMCC_TEMP_DIR=str(work / 'temp'))


def adapter(name):
    match = re.search(r'static void ' + name + r'\([^)]*\)\s*\{', source)
    assert match, name
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


names = ['PlayerCountUpdate', 'TeamSelectionUpdate', 'DifficultySelectionUpdate']
for name in names:
    assert f'lbl_1_bss_2050.unk04 = {name};' in source
assert 'void (*unk04)(omObjData *, void *);' in source
# Keep this argument contract explicit: the legacy team handler ignores arg0.
team = source[source.index('void fn_1_1EE58(s32 arg0, UnkFunc1EE58Struct *arg1)\n{'):]
team = team[:team.index('\nvoid fn_1_1F32C(')]
assert len(re.findall(r'\barg0\b', team)) == 1
cpu = source[source.index('void fn_1_1DD74(s32 arg0, StructBss8_24 *arg1)\n{'):]
cpu = cpu[:cpu.index('\nvoid fn_1_1E608(')]
assert len(re.findall(r'\barg0\b', cpu)) == 1
character_field = 'void (*unk4)(omObjData *, struct struct_bss8_24 *);'
assert character_field in header
assert 'lbl_1_bss_1E10[i].unk4 = fn_1_1CEB0;' in source
assert 'lbl_1_bss_1E10[i].unk4 = CpuCharacterUpdate;' in source
names.append('CpuCharacterUpdate')
common = '''
typedef int s32;
typedef struct { int marker; } omObjData;
typedef struct { int marker; } UnkFunc1EE58Struct;
static omObjData object = {123};
static UnkFunc1EE58Struct state = {456};
static unsigned called, invalid;
void fn_1_19F5C(omObjData *value) { called |= 1; if (value != &object) invalid = 1; }
void fn_1_1EE58(s32 unused, UnkFunc1EE58Struct *value) {
 called |= 2; if (unused != 0 || value != &state) invalid = 1;
}
void fn_1_1F32C(omObjData *value) { called |= 4; if (value != &object) invalid = 1; }
'''
common += 'typedef struct struct_bss8_24 { ' + character_field + ' } StructBss8_24;\n'
common += '''
static StructBss8_24 character;
void fn_1_1CEB0(omObjData *value, StructBss8_24 *context) {
 called |= 8; if (value != &object || context != &character) invalid = 1;
}
void fn_1_1DD74(s32 unused, StructBss8_24 *context) {
 called |= 16; if (unused != 0 || context != &character) invalid = 1;
}
'''
fixed = common + '\n'.join(adapter(name) for name in names) + '''
typedef void (*Callback)(omObjData *, void *);
int run_callback_contract(void) {
 Callback volatile callback = PlayerCountUpdate;
 callback(&object, &state);
 callback = TeamSelectionUpdate;
 callback(&object, &state);
 callback = DifficultySelectionUpdate;
 callback(&object, &state);
 volatile StructBss8_24 *stored = &character;
 stored->unk4 = fn_1_1CEB0;
 stored->unk4(&object, &character);
 stored->unk4 = CpuCharacterUpdate;
 stored->unk4(&object, &character);
 return invalid || called != 31;
}
#ifndef __EMSCRIPTEN__
int main(void) { return run_callback_contract(); }
#endif
'''
baseline = common + '''
typedef void (*Callback)(omObjData *, ...);
int run_callback_contract(void) {
 Callback volatile callback = (Callback)(void *)fn_1_19F5C;
 callback(&object, &state);
 return invalid;
}
'''
baseline_context = common + '''
typedef void (*Callback)(omObjData *, ...);
int run_callback_contract(void) {
 Callback volatile callback = (Callback)(void *)fn_1_1CEB0;
 callback(&object, &character);
 return invalid;
}
'''
results = []
for name, text in [('baseline', baseline), ('baseline-context', baseline_context), ('fixed', fixed)]:
    c_file = work / (name + '.c')
    binary = work / (name + '.wasm')
    c_file.write_text(text)
    # This fixture has no libc or Emscripten runtime calls. Use the SDK's
    # compiler directly so the test measures Wasm indirect-call signatures,
    # without requiring standalone reactor/exception runtime exports.
    compiler = sdk / 'upstream/bin' / ('clang.exe' if os.name == 'nt' else 'clang')
    command = [str(compiler), '--target=wasm32',
               str(c_file), '-O0', '-nostdlib',
               '-Wl,--no-entry', '-Wl,--export=run_callback_contract', '-o', str(binary)]
    if name == 'fixed':
        command += ['-Werror=cast-function-type', '-Werror=incompatible-function-pointer-types']
    compiled = subprocess.run(command, env=environment, capture_output=True, text=True)
    (work / (name + '.build.log')).write_text(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0, name + ' compile failed; inspect build log'
    observed = subprocess.run([args.node, '-e', '''
const fs=require('node:fs');
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[1]),{});
 instance.exports._initialize?.();
 try{console.log(JSON.stringify({result:instance.exports.run_callback_contract(),trapped:false}));}
 catch(error){console.log(JSON.stringify({trapped:true,type:error.name,message:error.message}));}
})().catch(error=>{console.error(error);process.exitCode=1;});
''', str(binary)], capture_output=True, text=True)
    assert observed.returncode == 0, observed.stderr
    result = json.loads(observed.stdout)
    if name == 'baseline':
        assert result['trapped'] and 'signature mismatch' in result['message'], result
    elif name == 'baseline-context':
        assert result == dict(result=1, trapped=False), result
    else:
        assert result == dict(result=0, trapped=False), result
    results.append(dict(case=name, **result))
report = dict(status='VERIFIED', scope='Strict Wasm callback ABI regression; actual adapter source extracted, no game execution',
              adapters=names, results=results, productionGameVerified=False)
(work / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
