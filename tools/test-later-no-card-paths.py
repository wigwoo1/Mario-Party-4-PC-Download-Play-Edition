"""Execute exact production save callbacks with UI traps; not visual gameplay QA."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True)
parser.add_argument('--vs-dev-cmd', required=True)
args = parser.parse_args()
project = Path(__file__).resolve().parent.parent
output = Path(args.output).resolve()
output.mkdir(parents=True, exist_ok=False)

def extract(relative, name):
    source = (project / relative).read_text(encoding='utf-8')
    match = re.search(r'^(?:void|s32)\s+' + name + r'\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Production definition missing: ' + name)
    pos, depth = match.end(), 1
    while depth:
        if source[pos] == '{':
            depth += 1
        elif source[pos] == '}':
            depth -= 1
        pos += 1
    return source[match.start():pos], hashlib.sha256((project / relative).read_bytes()).hexdigest()

callbacks = [
    ('src/REL/mstory3Dll/result.c', 'fn_1_1712C', 3, True),
    ('src/REL/mstory3Dll/result.c', 'fn_1_1728C', 3, True),
    ('src/REL/mgmodedll/mgmode.c', 'fn_1_6C18', 0, False),
    ('src/REL/mpexDll/mpex.c', 'fn_1_33F8', 0, False),
]
setter, _ = extract('src/game/saveload.c', 'SLSaveFlagSet')
getter, save_hash = extract('src/game/saveload.c', 'SLSaveFlagGet')
tests = []
for relative, name, cleanup_count, ends_process in callbacks:
    body, source_hash = extract(relative, name)
    calls = set(re.findall(r'\b([A-Za-z_]\w*)\s*\(', body))
    allowed = {name, 'if', 'while', 'SLSaveFlagGet', 'HuPrcEnd', 'Hu3DAllKill', 'HuSprClose', 'HuSprInit'}
    # Erase unavailable UI arguments too: the branch must never evaluate them.
    traps = '\n'.join('#define ' + call + '(...) forbidden_ui()' for call in sorted(calls - allowed))
    harness = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
typedef int s32; typedef short s16;
#define TARGET_PC 1
#define VERSION_REV1 1
#define TRUE 1
static struct { int party_continue, story_continue; } GWGameStat;
static struct { int save_mode; } GWSystem;
static struct { int active_pad; } winData[64];
static int SaveEnableF, saveExecF, lbl_1_bss_18D4, cleanups, terminated;
static jmp_buf process_end;
static int forbidden_ui(void) { puts("FORBIDDEN SAVE UI REACHED"); exit(41); return 0; }
static void Hu3DAllKill(void) { ++cleanups; }
static void HuSprClose(void) { ++cleanups; }
static void HuSprInit(void) { ++cleanups; }
static void HuPrcEnd(void) { terminated = 1; longjmp(process_end, 1); }
'''
    main = f'''
int main(void) {{
  for (int requested = -1; requested <= 2; ++requested) {{
    SaveEnableF = requested; GWGameStat.party_continue = GWGameStat.story_continue = 1;
    saveExecF = 1; lbl_1_bss_18D4 = 0; cleanups = terminated = 0;
    if (!setjmp(process_end)) {name}();
    assert(SaveEnableF == 0);
    assert(GWGameStat.party_continue == 0 && GWGameStat.story_continue == 0);
    assert(cleanups == {cleanup_count});
    assert(terminated == {int(ends_process)});
    {'assert(lbl_1_bss_18D4 == 1);' if ends_process else ''}
    {'assert(saveExecF == 0);' if name == 'fn_1_1712C' else ''}
  }}
  puts("PRODUCTION NO-CARD CALLBACK PASS"); return 0;
}}
'''
    for mutation in (False, True):
        variant = name + ('-guard-disabled' if mutation else '-production')
        test_body = body
        if mutation:
            # Ensure this test detects a lost guard rather than merely compiling.
            pattern = r'SLSaveFlagGet\(\)\s*==\s*0|!SLSaveFlagGet\(\)|SLSaveFlagGet\(\)\s*!=\s*0'
            test_body, count = re.subn(pattern, '1' if name == 'fn_1_33F8' else '0', body, count=1)
            assert count == 1
        c_file = output / (variant + '.c')
        exe_file = output / (variant + '.exe')
        c_file.write_text(harness + '\n' + setter + '\n' + getter + '\n' + traps + '\n' + test_body + '\n' + main)
        command = output / (variant + '.cmd')
        command.write_text('@echo off\ncall "' + args.vs_dev_cmd + '" -arch=x64 -host_arch=x64 >nul\n'
                           + 'cl /nologo /TC /Od /W0 "' + str(c_file) + '" /Fe:"' + str(exe_file) + '" /Fo:"' + str(output / (variant + '.obj')) + '"\n')
        compiled = subprocess.run(['cmd.exe', '/d', '/c', str(command)], capture_output=True, text=True, timeout=60)
        (output / (variant + '-build.log')).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError('Fixture compilation failed: ' + str(output / (variant + '-build.log')))
        run = subprocess.run([str(exe_file)], capture_output=True, text=True, timeout=10)
        expected = 41 if mutation else 0
        assert run.returncode == expected, (variant, run.returncode, run.stdout, run.stderr)
        tests.append(dict(callback=name, source=relative, sourceSha256=source_hash,
                          guardDisabled=mutation, exitCode=run.returncode, expectedExitCode=expected,
                          stdout=run.stdout.strip(), exactProductionFunction=not mutation))

report = dict(status='VERIFIED scoped production callback branches', tests=tests,
              saveFlagSourceSha256=save_hash, callbacksVerified=4, guardMutationsDetected=4,
              scope='Exact extracted production function bodies compiled as C with production save-flag getter/setter; UI calls trap, process termination modeled with longjmp',
              normalGameplayTransitionVerified=False, visualCardScreenAbsenceVerified=False,
              nativeGameDllExecuted=False, antivirusProtectionsChanged=False)
(output / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: value for key, value in report.items() if key != 'tests'}))
