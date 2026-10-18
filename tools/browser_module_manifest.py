"""Inventory overlay IDs and source groups for planning a browser module loader.

This is a source inventory, not preprocessing, linking or a WebAssembly build.
"""
import argparse
import json
from pathlib import Path
import re


def inventory(repo):
    table = (repo / 'include/ovl_table.h').read_text()
    # This inventory targets the current USA PC build, not all regional branches.
    conditions, selected = [], []
    for line in table.splitlines():
        directive = line.strip()
        if directive in ('#ifdef TARGET_PC', '#if VERSION_JP'):
            conditions.append(directive == '#ifdef TARGET_PC')
        elif directive == '#else':
            conditions[-1] = not conditions[-1]
        elif directive == '#endif':
            conditions.pop()
        elif directive.startswith(('#if ', '#ifdef ', '#ifndef ', '#elif ')):
            raise ValueError(f'Unsupported table condition: {directive}')
        elif all(conditions):
            selected.append(line)
    if conditions:
        raise ValueError('Unbalanced overlay conditions')
    table = '\n'.join(selected)
    names = re.findall(r'^\s*DLL\(\s*(\w+)\s*\)', table, re.M)
    if not names or len(names) != len(set(names)):
        raise ValueError('Overlay table must contain unique named entries')
    sources = re.findall(r'\bsrc/REL/[\w/]+\.c\b', (repo / 'files.cmake').read_text())
    available = [p.relative_to(repo).as_posix() for p in (repo / 'src/REL').rglob('*.c')]
    result = []
    for index, name in enumerate(names):
        group = sorted(set(p for p in sources if p.casefold().startswith(f'src/rel/{name.casefold()}/')))
        board = bool(re.fullmatch(r'w..dll', name.casefold()))
        if board:
            group.append('src/REL/board_executor.c')
        discovered = sorted(p for p in available if p.casefold().startswith(f'src/rel/{name.casefold()}/'))
        status = 'listed_in_build' if group else ('source_not_in_build' if discovered else 'no_c_source_group')
        result.append({'overlay_id': index, 'name': name, 'sources': group,
                       'source_status': status, 'unlisted_sources': [p for p in discovered if p not in group],
                       'board_shared_entry': board,
                       'missing_sources': [p for p in group if not (repo / p).is_file()]})
    return {'scope': 'Source inventory only; no browser build or symbol-collision analysis',
            'module_count': len(result), 'modules': result,
            'unmapped_modules': sorted({p.split('/')[2] for p in sources if p.count('/') >= 3
                                        and p.split('/')[2].casefold() not in {n.casefold() for n in names}})}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = inventory(args.repo)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Inventoried {report['module_count']} overlay IDs; source manifest saved")
