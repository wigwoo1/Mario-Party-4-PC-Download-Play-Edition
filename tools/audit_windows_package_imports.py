"""Inventory regular/delay PE imports of a measured package without loading it.

External names remain unclassified: presence on the development PC does not prove
availability on a clean Windows installation. Dynamic LoadLibrary calls and legal
redistribution requirements are outside this audit.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess


def pe_directories(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Expected DOS/PE header')
    start = struct.unpack_from('<I', data, 60)[0]
    if start + 24 > len(data) or data[start:start+4] != b'PE\0\0':
        raise ValueError('Bounded PE signature required')
    machine = struct.unpack_from('<H', data, start+4)[0]
    optional_size = struct.unpack_from('<H', data, start+20)[0]
    optional = start + 24
    if machine != 0x8664 or optional_size < 224 or optional + optional_size > len(data):
        raise ValueError('Bounded AMD64 PE32+ optional header required')
    if struct.unpack_from('<H', data, optional)[0] != 0x20b:
        raise ValueError('PE32+ required')
    count = struct.unpack_from('<I', data, optional+108)[0]
    if count < 14:
        raise ValueError('Import/delay directory bounds unavailable')
    return {kind for kind, index in [('regular', 1), ('delay', 13)]
            if struct.unpack_from('<I', data, optional+112+8*index)[0]}


def parse_dependents(text, expected):
    headings = {
        'Image has the following dependencies:': 'regular',
        'Image has the following delay load dependencies:': 'delay',
    }
    mode = None
    seen = set()
    result = []
    for raw in text.splitlines():
        line = raw.strip()
        if line in headings:
            mode = headings[line]
            seen.add(mode)
        elif line == 'Summary':
            mode = None
        elif mode and line:
            if not re.fullmatch(r'[A-Za-z0-9_.+\-]+\.(?:dll|exe|drv)', line, re.I):
                raise ValueError('Unexpected import name or dumpbin output')
            result.append({'name': line, 'kind': mode})
    if seen != expected or any(not any(r['kind'] == k for r in result) for k in expected):
        raise ValueError('Import directory and dumpbin sections disagree')
    if 'File Type:' not in text or 'Summary' not in text:
        raise ValueError('Unrecognized dumpbin output')
    return result


def audit(package, dumpbin):
    package = Path(package).resolve()
    manifest = json.loads((package/'package-manifest.json').read_text())
    pe_entries = [e for e in manifest['files'] if Path(e['path']).suffix.lower() in ['.exe', '.dll']]
    names = {}
    for entry in pe_entries:
        key = Path(entry['path']).name.lower()
        if key in names:
            raise ValueError('Ambiguous packaged module basename')
        names[key] = entry['path']
    modules = []
    for entry in pe_entries:
        candidate = package/entry['path']
        path = candidate.resolve()
        if not path.is_relative_to(package) or any(
                part.is_symlink() or (hasattr(part, 'is_junction') and part.is_junction())
                for part in [candidate, *candidate.parents] if part != package):
            raise ValueError('Package module escaped root')
        data = path.read_bytes()
        if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('Measured module changed')
        expected = pe_directories(data)
        result = subprocess.run([str(dumpbin), '/nologo', '/dependents', str(path)],
                                capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=30, check=True)
        imports = parse_dependents(result.stdout, expected)
        for row in imports:
            target = names.get(row['name'].lower())
            row['packagedPath'] = target
            row['resolution'] = 'external-unclassified'
            if target:
                accessible = Path(target).parent in [Path(entry['path']).parent, Path('runtime')]
                row['resolution'] = 'bundled-search-directory' if accessible else 'bundled-outside-default-search'
        modules.append({'path': entry['path'], 'sha256': entry['sha256'], 'machine': 'AMD64', 'imports': imports})
    external = sorted({i['name'].lower() for m in modules for i in m['imports'] if i['packagedPath'] is None})
    return {'status': 'VERIFIED measured PE import inventory; external prerequisite classification pending',
            'buildId': manifest['buildId'], 'modules': modules, 'moduleCount': len(modules),
            'externalImports': external,
            'bundledOutsideSearch': [dict(importer=m['path'], **i) for m in modules for i in m['imports']
                                     if i['resolution'] == 'bundled-outside-default-search'],
            'gameExecuted': False, 'dynamicLoadingCovered': False,
            'cleanMachineVerified': False, 'redistributionAuditComplete': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package')
    parser.add_argument('report')
    parser.add_argument('--dumpbin', default=shutil.which('dumpbin'))
    args = parser.parse_args()
    if not args.dumpbin:
        parser.error('Microsoft dumpbin path required')
    report = Path(args.report)
    if report.exists():
        parser.error('Preserve prior evidence; use a fresh report path')
    result = audit(args.package, args.dumpbin)
    report.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: result[k] for k in ['status', 'moduleCount', 'externalImports', 'bundledOutsideSearch']}))
