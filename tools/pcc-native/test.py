#!/usr/bin/env python3
"""Compare host/native compiler passes and execute their s.out output in Unix."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image,compile_c,run
from selfhost import Filesystem
PCC=ROOT/'PCC-z8000/z8000'
PASSES=ROOT/'tests/build/native-pcc'
WORK=ROOT/'tests/build/native-pcc-trials'
SYS=ROOT/'v7z8000/usr/sys/build'
RUNTIME=ROOT/'tests/build/sout-cc'
AS=ROOT/'tests/build/asz8k-host/asz8k'
LD=ROOT/'tests/build/ldz8-host/ldz8'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('suite',nargs='?',choices=['extra'])
parser.add_argument('--native-front',type=Path,default=PASSES/'target-front/front')
parser.add_argument('--native-back',type=Path,default=PASSES/'target-back/back')
args=parser.parse_args()
WORK.mkdir(parents=True,exist_ok=True)
compile_c(Path(__file__).with_name('runner.c'),WORK/'passrun.b')
run([LD,'-s',RUNTIME/'crt0.b',WORK/'passrun.b',RUNTIME/'libc.a','-o',WORK/'passrun'])
cases='hello arith control switch bitfield shift larith pcc_math pcc_cmp pcc_struct pcc_structret pcc_union pcc_ptr pcc_scope pcc_optim'.split()
if args.suite=='extra':
    cases='float_general float_storage float_add float_convert_vectors double_with_regvars register_long register_calls long_postinc register_pair_overlap widen_pressure address_bytes'.split()
records=[]
for name in cases:
    source=PCC/'test'/('regress' if args.suite=='extra' else '')/(name+'.c')
    src=source.read_bytes();(WORK/'input').write_bytes(src)
    middle=run([PASSES/'front'],input=src).stdout
    expected=run([PASSES/'back'],input=middle).stdout
    image({'bin/front':args.native_front,'bin/back':args.native_back,
           'bin/passrun':WORK/'passrun','tmp/input':WORK/'input'},WORK/'hd.img')
    result=run([SYS/'test_driver','-c','3000000000','-d',WORK/'hd.img',
        '-i','passrun\\n','-w','OUTPUT END','-I','exit\\n','-x','OUTPUT END'],cwd=SYS,timeout=90)
    (WORK/(name+'.log')).write_bytes(result.stdout+result.stderr)
    encoded=result.stdout.split(b'OUTPUT BEGIN\r\n',1)[1].split(b'OUTPUT END',1)[0]
    actual=bytes.fromhex(encoded.decode());(WORK/(name+'.az8')).write_bytes(actual)
    (WORK/(name+'.expected')).write_bytes(expected)
    (WORK/'asz8k.pd').write_bytes((ROOT/'tools/asz8k/src/asz8k.pd').read_bytes())
    (WORK/'input.az8').write_bytes(actual)
    run([AS,'-c','-o',name+'.b','input.az8'],cwd=WORK)
    run([LD,'-i','-s',RUNTIME/'crt0.b',WORK/(name+'.b'),RUNTIME/'libc.a','-o',WORK/(name+'.out')])
    plan=WORK/'plan';plan.write_text('0 - /bin/passrun /bin/probe %d\n'%{'hello':42,'arith':120}.get(name,0))
    image({'bin/runner':ROOT/'tests/build/native-cc-sout/runner',
           'bin/probe':WORK/(name+'.out'),'bin/passrun':WORK/'passrun','tmp/plan':plan},WORK/'execute.img')
    result=run([SYS/'test_driver','-c','3000000000','-d',WORK/'execute.img',
        '-i','runner /tmp/plan /tmp\\n','-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,timeout=90)
    (WORK/(name+'-execute.log')).write_bytes(result.stdout+result.stderr)
    assert b'NATIVE CC PASS\r\n' in result.stdout,name
    # Host strtod and V7 atof may round decimal constants differently.
    records.append({'name':name,'identical':actual==expected,'execute':True,'format':'s.out'})
    print(records[-1],flush=True)
    (WORK/('extra-results.json' if args.suite else 'results.json')).write_text(json.dumps(records,indent=2)+'\n')
