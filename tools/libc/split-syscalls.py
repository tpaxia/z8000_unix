#!/usr/bin/env python3
"""Emit separately selectable syscall archive members from the assembly source."""
from pathlib import Path
import re
import sys
source = Path(__file__).with_name('syscalls.az8').read_text()
source = re.sub(r'^\s*\.globl[^\n]*\n', '', source, flags=re.M)
functions = list(re.finditer(r'^(_\w+):', source, re.M))
parts = {}
for i, match in enumerate(functions):
    symbol = match[1]
    if symbol == '_curbrk': continue
    body = source[match.start():functions[i+1].start() if i+1<len(functions) else len(source)]
    parts['s_'+symbol[1:]] = ('\t.text\n\t.globl '+symbol+'\n\t.comm _errno,2\n'
        'cerror:\n\tld _errno,r1\n\tret\n'+body)
parts['s_curbrk'] = '\t.data\n\t.even\n\t.globl _curbrk\n_curbrk:\n\t.word _end\n'
if '--names' in sys.argv:
    print(' '.join(parts))
else:
    destination = Path(sys.argv[1]); destination.mkdir(parents=True,exist_ok=True)
    for name,text in parts.items():
        p=destination/(name+'.az8')
        if not p.exists() or p.read_text()!=text:p.write_text(text)
