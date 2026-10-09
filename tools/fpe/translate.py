#!/usr/bin/env python3
"""Prepare the preserved Zilog arithmetic/decoder for the Unix service.

The arithmetic/decoder is unchanged. Unix supplies the entry adapter instead
of fp_epu, and passes a per-process workspace to the original epu routine.
"""
import re
import sys
from pathlib import Path

s = Path(sys.argv[1]).read_text()
s = s[:s.index('frm_set\t.equ')] + '\n.text\n' + s[s.index('\nepu:\n'):]
out = ['.unsegm', '.global epu']
for line in s.splitlines():
    line = line.split(';')[0].strip().rstrip(',')
    if not line or line.lower().startswith(('.input', '.eject')):
        continue
    line = re.sub(r'__text\s+\.sect', '.text', line, flags=re.I)
    line = re.sub(r'__data\s+\.sect', '.data', line, flags=re.I)
    line = re.sub(r'\b([0-9][0-9a-fA-F]*)h\b', lambda m: '0x'+m[1], line)
    line = re.sub(r'\b(r\d+)\(#([^)]*)\)', r'\2(\1)', line)
    line = re.sub(r'\.block\b', '.space', line, flags=re.I)
    if line == 'Fsetmode':
        line += ':'
    if re.fullmatch(r'(rrc?b?|rlc?b?|incb?|decb?)\s+r[hl]?\d+', line):
        line += ',#1'
    out.append(line)
Path(sys.argv[2]).write_text('\n'.join(out)+'\n')
