#!/usr/bin/env python3
"""Account for every top-level V7 command source and report build limits."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
CMD = ROOT / 'v7z8000/usr/src/cmd'
WORK = ROOT / 'tests/build/userland-all'
report = json.loads((WORK / 'report.json').read_text())
groups = {
    'as': ['asz8k'], 'c': ['front', 'back'], 'cpp': ['cpp'],
    'pcc': ['front', 'back'], 'mip': ['lint1'], 'sh': ['sh'],
    'learn': ['learn', 'lcount', 'learntee'], 'lint': ['lint1', 'lint2'],
    'plot': ['t300', 't300s', 'tek', 't450', 'vplot'],
    'refer': ['refer', 'mkey', 'inv', 'hunt', 'deliv'],
    'spell': ['spell', 'spellin', 'spellout'],
    'struct': ['structure', 'beautify'], 'troff': ['nroff', 'troff'],
    'uucp': ['uucp', 'uux', 'uuxqt', 'uucico', 'uulog', 'uuclean'],
    'xsend': ['enroll', 'xget', 'xsend'],
}
external = {'asz8k', 'front', 'back', 'cpp', 'sh'}
special = {
    'standalone': 'Boot and standalone machine programs; outside userland.',
    'cmake': 'Original build script; replaced by the Z8000 build recipes.',
    'in': 'Original build list; covered by the inventory.',
    'makeall': 'Original build script; replaced by tools/userland/run.py.',
    'prep.h': 'Header used by prep.',
    'num56.h': 'Exact 56-bit arithmetic shared by factor and primes.',
    'num56.az8': 'Z8000 limb division shared by factor and primes.',
}
rows = []
for path in sorted(CMD.iterdir()):
    name = path.stem if path.is_file() and path.suffix in ('.c', '.s', '.y') else path.name
    if path.name in special:
        rows.append({'source':path.name, 'status':'support', 'reason':special[path.name]})
        continue
    outputs = groups.get(name, [name])
    missing = [n for n in outputs if n not in report and n not in external]
    if missing:raise SystemExit('Unaccounted source '+path.name+': '+', '.join(missing))
    rows.append({'source':path.name, 'outputs':outputs,
                 'status':{n:report[n]['status'] if n in report else 'existing-toolchain' for n in outputs}})
summary = {
    'source_units':rows,
    'commands_built':sorted(n for n,r in report.items() if r['status']=='built' and r.get('kind')=='command'),
    'games_built':sorted(n for n,r in report.items() if r['status']=='built' and r.get('kind')=='game'),
    'libraries_built':sorted(n for n,r in report.items() if r['status']=='built' and n.startswith('lib')),
    'terminal_tables_built':sorted(n for n,r in report.items() if r['status']=='built' and r.get('kind')=='terminal-table'),
    'not_installed':{
        'init':'Built as /etc/init.v7; console init remains the active boot program.',
    },
    'unported':{n:r['reason'] for n,r in report.items() if r['status']=='unported'},
    'replaced':{n:r['reason'] for n,r in report.items() if r['status']=='replaced'},
}
summary['game_sources']={p.name: report[p.stem if p.is_file() else p.name]['status']
                         for p in sorted((ROOT/'v7z8000/usr/src/games').iterdir())}
summary['game_binaries_without_source']=sorted(p.name for p in (ROOT/'v7unix/usr/games').iterdir()
    if p.is_file() and b'\0' in p.read_bytes() and p.name not in report)
summary['game_scripts_not_installed']={
    'ching':'Requires the unavailable cno and phx helpers.',
    'words':'Requires the unavailable words1 program.',
}
summary['library_sources']={
    'libc':'Existing Z8000 libc; rebuilt separately by the native environment.',
    'libdbm':['libdbm'], 'libF77':['libF77'], 'libI77':['libI77'],
    'libfpsim':'PDP-11 assembly; replaced by the separate Z8000 EPU implementation.',
    'libm':['libm'], 'libmp':['libmp'],
    'libplot':['libplot','libt300','libt300s','libt4014','libt450','libvt0'],
}
for directory in (ROOT/'v7z8000/usr/src').glob('lib*'):
    if directory.name not in summary['library_sources']:
        raise SystemExit('Unaccounted library source '+directory.name)
(WORK / 'inventory.json').write_text(json.dumps(summary, indent=2)+'\n')
print('Inventory:',len(rows),'source units;',len(summary['commands_built']),'commands built;',
      len(summary['games_built']),'games;',len(summary['libraries_built']),'libraries;',len(summary['terminal_tables_built']),'terminal tables')
for name,reason in summary['unported'].items():print(name+': '+reason)
