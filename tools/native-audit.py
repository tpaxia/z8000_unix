#!/usr/bin/env python3
"""Audit current s.out bootstrap and native environment resource limits."""
from pathlib import Path
import json
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from object_format import sizes
from selfhost import Filesystem
WORK=ROOT/'tests/build/native-audit';WORK.mkdir(parents=True,exist_ok=True)
report={}
for generation,base in [('bootstrap',ROOT/'tests/build/native-cc-sout'),
                        ('native',ROOT/'tests/build/native-environment-sout')]:
    fs=Filesystem(base/'hd.img');programs={}
    for path in ('/bin/cc','/bin/asz8k','/bin/ldz8','/lib/front','/lib/back','/lib/oz8','/lib/cpp'):
        resource=sizes(fs.read(path))
        assert resource['text']<=65535 and resource['data']+resource['bss']<65536,path
        programs[path]=resource
    report[generation]=programs
(WORK/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS s.out bootstrap/native resource audit')
