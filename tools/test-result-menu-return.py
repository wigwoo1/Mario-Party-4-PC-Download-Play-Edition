"""Check the exact results-menu return contract without loading game DLLs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True)
parser.add_argument('--vs-dev-cmd', required=True)
parser.add_argument('--expect-missing-return', action='store_true')
args = parser.parse_args()
source_file = Path(__file__).resolve().parent.parent / 'src/REL/mstory3Dll/result.c'
source = source_file.read_text(encoding='utf-8')
start = source.index('s32 fn_1_16924(void) {')
pos, depth = source.index('{', start) + 1, 1
while depth:
    depth += (source[pos] == '{') - (source[pos] == '}')
    pos += 1
body = source[start:pos]
output = Path(args.output).resolve()
output.mkdir(parents=True, exist_ok=False)
calls = set(re.findall(r'\b([A-Za-z_]\w*)\s*\(', body))
real = {'if', 'while', 'fn_1_16924', 'fn_1_938', 'fn_1_1834'}
macros = '\n'.join('#define ' + name + '(...) 0' for name in sorted(calls - real))
harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef int s32;
#define TRUE 1
#define PAD_BUTTON_A 0x100
#define PAD_BUTTON_MENU 0x1000
#define WIPE_MODE_IN 0
#define WIPE_MODE_OUT 1
#define WIPE_TYPE_CROSS 0
static struct { int unk14, unk04, unk0C, unk10; struct { int unk14; } unk38[4]; } lbl_1_bss_1A0C;
static unsigned HuPadBtnDown[4], requested;
static int ticks, details;
static void fn_1_938(void) {
    ++ticks; if (ticks > 400) exit(42);
    HuPadBtnDown[0] = ticks >= 5 ? requested : 0;
}
static int fn_1_1834(int message, int style) { ++details; return 7; }
'''
main = r'''
static void check(unsigned button, int human, int expected, int expectedDetails) {
    requested = button; ticks = details = 0;
    lbl_1_bss_1A0C.unk14 = human ? 0 : -1;
    lbl_1_bss_1A0C.unk04 = 0; lbl_1_bss_1A0C.unk38[0].unk14 = 0;
    const int result = fn_1_16924();
    assert(result == expected); assert(details == expectedDetails);
    assert(ticks >= 5); if (!human) assert(ticks == 301);
}
int main(void) {
    check(PAD_BUTTON_A, 1, 0, 1);
    check(PAD_BUTTON_MENU, 1, 1, 0);
    check(0, 0, 1, 0);
    check(PAD_BUTTON_A | PAD_BUTTON_MENU, 1, 0, 1);
    puts("RESULT MENU A/START/AUTO/A-PRECEDENCE RETURN CONTRACT PASS");
    return 0;
}
'''
c_file, exe = output / 'contract.c', output / 'contract.exe'
c_file.write_text(harness + '\n' + macros + '\n' + body + '\n' + main, encoding='utf-8')
command = output / 'compile.cmd'
command.write_text('@echo off\ncall "' + args.vs_dev_cmd + '" -arch=x64 -host_arch=x64 >nul\n'
                   + 'cl /nologo /TC /Od /W4 /we4715 /we4716 "' + str(c_file) + '" /Fe:"' + str(exe) + '" /Fo:"' + str(output / 'contract.obj') + '"\n')
compiled = subprocess.run(['cmd.exe', '/d', '/c', str(command)], capture_output=True, text=True, timeout=60)
log = compiled.stdout + compiled.stderr
(output / 'build.log').write_text(log)
report = dict(sourceSha256=hashlib.sha256(source_file.read_bytes()).hexdigest(),
              functionSha256=hashlib.sha256(body.encode()).hexdigest(), compilerExitCode=compiled.returncode,
              exactProductionFunction=True, gameDllExecuted=False, scope='Extracted production function; input/window/animation stubs, not full gameplay')
if args.expect_missing_return:
    assert compiled.returncode != 0 and re.search(r'C471[56]', log), log
    report.update(status='VERIFIED baseline missing-return compile failure', expectedFailure=True)
else:
    assert compiled.returncode == 0, log
    ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert ran.returncode == 0, (ran.returncode, ran.stdout, ran.stderr)
    report.update(status='VERIFIED four return paths', executableExitCode=ran.returncode, cases=4, stdout=ran.stdout.strip())
(output / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
