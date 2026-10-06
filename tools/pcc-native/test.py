from pathlib import Path
import subprocess,struct,json,sys,argparse
root=Path(__file__).resolve().parents[2];w=root/"tests/build/native-pcc"; proto=w;pcc=root/'PCC-z8000/z8000';build=root/'v7z8000/usr/sys/build'
parser=argparse.ArgumentParser(description='Exercise native compiler passes under Unix')
parser.add_argument('suite',nargs='?',choices=['extra'])
parser.add_argument('--native-front',type=Path,default=w/'target-front/front')
parser.add_argument('--native-back',type=Path,default=w/'target-back/back')
args=parser.parse_args()
def run(cmd,**kw): return subprocess.run(list(map(str,cmd)),capture_output=True,check=True,timeout=60,**kw)
# Generated programs execute in the standalone CPU harness, which has no
# Unix EPU service. Keep its reference runtime separate from Unix libc.
run(['make','-C',pcc/'test','softfp.b','csv.b'])
r=run(['cpp','-nostdinc','-undef','-I'+str(root/'v7z8000/usr/include'),Path(__file__).with_name('runner.c')])
r=run([pcc/'cz8/cz8'],input=r.stdout);(w/'runner.az8').write_bytes(r.stdout)
run([pcc/'az8/az8','-o','runner.b','runner.az8'],cwd=w)
run([pcc/'ldz8','-x',root/'tools/libc/crt0.b',w/'runner.b',root/'tools/libv7.a','-o',w/'runner'])
cases=['hello','arith','control','switch','bitfield','shift','larith','pcc_math','pcc_cmp','pcc_struct','pcc_structret','pcc_union','pcc_ptr','pcc_scope','pcc_optim']
extra=args.suite=='extra'
if extra: cases="float_general float_storage float_add float_convert_vectors double_with_regvars register_long register_calls long_postinc register_pair_overlap widen_pressure".split()
records=[]
for name in cases:
 src=(pcc/'test'/('regress' if extra else '')/(name+'.c')).read_bytes()
 ir=run([proto/'front'],input=src).stdout;(w/'input').write_bytes(src)
 expected=run([proto/'back'],input=ir).stdout
 (w/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
 sh ---755 0 0 {root}/tools/sh
 back ---755 0 0 {args.native_back.resolve()}
 front ---755 0 0 {args.native_front.resolve()}
 runner ---755 0 0 {w}/runner
 $
dev d--755 0 0
 console c--644 0 0 0 0
 tty c--644 0 0 2 0
 $
etc d--755 0 0
 init ---755 0 0 {root}/tools/init
 $
tmp d--777 0 0
 input ---644 0 0 {w}/input
 $
$
''')
 run([root/'tools/v7mkfs',w/'hd.img',w/'proto'])
 r=subprocess.run([str(build/'test_driver'),'-c','900000000','-d',str(w/'hd.img'),'-i','runner\\n','-w','OUTPUT END','-I','exit\\n','-x','OUTPUT END'],cwd=build,capture_output=True,timeout=60)
 (w/(name+'.log')).write_bytes(r.stdout+r.stderr)
 assert r.returncode==0,(name,r.stdout[-2000:])
 out=r.stdout.split(b'OUTPUT BEGIN\r\n',1)[1].split(b'OUTPUT END',1)[0]
 actual=bytes.fromhex(out.decode());(w/(name+'.az8')).write_bytes(actual)
 (w/(name+'.expected')).write_bytes(expected)
 rec={'name':name,'identical':actual==expected}
 # V7 atof can round decimal constants differently from host strtod;
 # retain differences and execute every generated program below.
 run([pcc/'az8/az8','-o',name+'.b',name+'.az8'],cwd=w)
 run([pcc/'ldz8','-x',pcc/'test/crt0.b','-R','8',w/(name+'.b'),pcc/'test/liblong.b',pcc/'test/libfloat.b',pcc/'test/softfp.b',pcc/'test/exit.b',pcc/'test/csv.b','-o',w/(name+'.bout')])
 r=run([pcc/'test/run_emu',w/(name+'.bout'),'-e',str({'hello':42,'arith':120}.get(name,0)),'-c','10000000'])
 (w/(name+'-execute.log')).write_bytes(r.stdout+r.stderr)
 rec['execute']=True; records.append(rec);print(rec,flush=True)
 (w/('extra-results.json' if extra else 'results.json')).write_text(json.dumps(records,indent=2))
