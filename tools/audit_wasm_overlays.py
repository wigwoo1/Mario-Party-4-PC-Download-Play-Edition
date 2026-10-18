"""Compile listed USA overlays to Wasm objects and report cross-overlay symbols.

This does not link or execute overlays. Objects and compiler logs remain in --work.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys
from browser_module_manifest import inventory


def audit(repo, sdk, work):
    manifest = inventory(repo)
    sources = sorted({s for m in manifest['modules'] for s in m['sources']})
    env = dict(os.environ, EM_CONFIG=str(sdk / '.emscripten'))
    includes = ['include', 'extern', 'extern/aurora/include', 'extern/aurora/include/dolphin', 'extern/musyx/include']
    flags = ['-c', '-DTARGET_PC', '-DNON_MATCHING', '-DVERSION=1',
             '-DOPTIMIZED_TEXTURE_LOADING', '-DBYTESWAPPING', '-DMUSY_TARGET=MUSY_TARGET_PC']
    flags += ['-I' + str(repo / p) for p in includes]
    nm = sdk / 'upstream/bin/llvm-nm.exe'
    if not nm.exists():
        nm = sdk / 'upstream/bin/llvm-nm'
    def compile_source(source):
        target = work / (source.replace('/', '_') + '.o')
        result = subprocess.run([sys.executable, str(sdk / 'upstream/emscripten/emcc.py'),
                                 str(repo / source), *flags, '-o', str(target)],
                                env=env, capture_output=True, text=True, timeout=120)
        log = target.with_suffix('.log')
        log.write_text(result.stdout + result.stderr)
        report = {'source': source, 'compiled': result.returncode == 0, 'log': str(log), 'symbols': []}
        if result.returncode == 0:
            symbols = subprocess.run([str(nm), '--defined-only', '--extern-only', '--format=posix',
                                      str(target)], check=True, capture_output=True, text=True, timeout=30)
            report['symbols'] = sorted({line.split()[0] for line in symbols.stdout.splitlines() if line.strip()})
        return report
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(compile_source, sources))
    by_source = {r['source']: r for r in results}
    definitions = {}
    modules = []
    for module in manifest['modules']:
        available = module['sources']
        ready = bool(available) and all(by_source[s]['compiled'] for s in available)
        modules.append({'name': module['name'], 'all_sources_compiled': ready,
                        'failed_sources': [s for s in available if not by_source[s]['compiled']],
                        'source_status': module['source_status']})
        for symbol in {name for s in available for name in by_source[s]['symbols']}:
            definitions.setdefault(symbol, []).append(module['name'])
    collisions = {s: sorted(set(names)) for s, names in sorted(definitions.items()) if len(set(names)) > 1}
    return {'scope': 'Compile-only Wasm source/symbol audit; no linking, loading or game execution',
            'source_count': len(sources), 'compiled_count': sum(r['compiled'] for r in results),
            'module_count': len(modules), 'modules_compiled': sum(m['all_sources_compiled'] for m in modules),
            'collision_count': len(collisions), 'collisions': collisions, 'modules': modules, 'sources': results}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=False)
    report = audit(args.repo.resolve(), args.sdk.resolve(), args.work.resolve())
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Compiled {report['compiled_count']}/{report['source_count']} sources; "
          f"{report['modules_compiled']}/{report['module_count']} overlays; "
          f"{report['collision_count']} cross-overlay symbol collisions")



