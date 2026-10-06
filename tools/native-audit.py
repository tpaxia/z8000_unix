#!/usr/bin/env python3
"""Cross-build native PCC tools without modifying their sources.

Run from any directory after building tools/libv7.a and the kernel. Results,
per-stage logs, generated parser and objects go to tests/build/native-audit.
Known compilation failures are recorded, not hidden by source workarounds.
This is an audit, not a self-hosting success test: inspect report.json.
"""
import importlib.util, json, pathlib, re, shutil, struct, subprocess, sys
sys.dont_write_bytecode = True
root=pathlib.Path(__file__).resolve().parent.parent
pcc=root/'PCC-z8000/z8000'
v7=root/'v7z8000'
work=root/'tests/build/native-audit'
work.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('regen',pcc/'cz8/regen_cgram.py')
regen=importlib.util.module_from_spec(spec);spec.loader.exec_module(regen)
ywork=work/'yacc';ywork.mkdir(exist_ok=True)
yacc=regen.build_yacc(ywork)
cppsrc=v7/'usr/src/cmd/cpp'
shutil.copy(cppsrc/'cpy.y',ywork/'cpy.y')
r=subprocess.run([str(yacc),'cpy.y'],cwd=ywork,capture_output=True)
(ywork/'generate.log').write_bytes(r.stdout+r.stderr)
assert r.returncode==0
sources={
 'cpp':[cppsrc/'cpp.c',ywork/'y.tab.c'],
 'cz8':[pcc/'cz8'/f'{n}.c' for n in 'cgram xdefs scan pftn trees optim code local reader local2 order match allo comm1 table'.split()],
 'az8':[pcc/'az8'/f'{n}.c' for n in 'error init ins ioz8 ps rel sdi sym scan'.split()],
 'ldz8':[pcc/'ldz8.c'],
 'ccz8':[pcc/'ccz8.c'],
}
report={}
# Ensure libc and startup objects are current; no toolchain source edits.
subprocess.run(['make', '-C', str(root/'tools'), 'libv7.a', 'libc/crt0.b',
                'v7mkfs', 'init', 'sh', 'cat'], check=True)
for tool,files in sources.items():
 d=work/tool;d.mkdir(exist_ok=True)
 records=[]
 (d/tool).unlink(missing_ok=True)
 (d/'link.log').unlink(missing_ok=True)
 for src in files:
  name=src.stem
  # A failed rerun must never reuse an older object or executable.
  for suffix in ('.i', '.az8', '.b'):
   (d/(name+suffix)).unlink(missing_ok=True)
  rec={'source':str(src.relative_to(root))}
  commands=[['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-Dunix=1','-I'+str(src.parent),'-I'+str(v7/'usr/include')]+(['-I'+str(cppsrc)] if tool=='cpp' else [])+[str(src)], [str(pcc/'cz8/cz8')],[str(pcc/'az8/az8'),'-o',name+'.b',name+'.az8']]
  data=None
  for stage,cmd in zip(['cpp','cz8','az8'],commands):
   r=subprocess.run(cmd,input=data if stage=='cz8' else None,cwd=d,capture_output=True,timeout=60)
   (d/(name+'.'+stage+'.log')).write_bytes(r.stderr+(r.stdout if stage=='az8' else b''))
   if stage=='cpp':(d/(name+'.i')).write_bytes(r.stdout)
   if stage=='cz8':(d/(name+'.az8')).write_bytes(r.stdout)
   if r.returncode:
    rec.update(status=stage+' failed',returncode=r.returncode,diagnostic=r.stderr.decode(errors='replace')[:2000]);break
   data=r.stdout
  else:
   vals=struct.unpack('>8H',(d/(name+'.b')).read_bytes()[:16]);rec.update(status='assembled',text=vals[1],data=vals[2],bss=vals[3])
  records.append(rec)
  print(tool,name,rec['status'],flush=True)
 # Object totals exclude libc and every source that failed. Common symbols
 # are deduplicated by their actual eight-byte on-disk names.
 totals={'text':0, 'data':0, 'bss':0}
 commons={}
 definitions=set()
 for src,rec in zip(files,records):
  if rec['status']!='assembled':continue
  raw=(d/(src.stem+'.b')).read_bytes()
  h=struct.unpack('>8H',raw[:16])
  for key,value in zip(totals,h[1:4]):totals[key]+=value
  offset=16+h[1]+h[2]+h[6]+h[7]
  for pos in range(offset,offset+h[4],12):
   symbol,typ,value=struct.unpack('>8sHH',raw[pos:pos+12])
   if typ==0o40 and value:
    commons[symbol]=max(commons.get(symbol,0),(value+1)&~1)
   elif typ & 0o40 and typ & 0o37:
    definitions.add(symbol)
 totals['common']=sum(v for k,v in commons.items() if k not in definitions)
 totals['total']=sum(totals.values())
 report[tool]={'files':records, 'compiled_object_totals':totals,
               'complete':all(rec['status']=='assembled' for rec in records)}
 if all(rec['status']=='assembled' for rec in records):
  cmd=[str(pcc/'ldz8'),'-x','-R','0']+(['-i'] if tool in ('az8','ldz8') else [])+[str(root/'tools/libc/crt0.b')]+[src.stem+'.b' for src in files]+[str(root/'tools/libv7.a'),'-o',tool]
  r=subprocess.run(cmd,cwd=d,capture_output=True,timeout=60)
  (d/'link.log').write_bytes(r.stdout+r.stderr)
  report[tool]['link_returncode']=r.returncode
  report[tool]['link_log']=(r.stdout+r.stderr).decode(errors='replace')
  if (d/tool).exists():
   vals=struct.unpack('>8H',(d/tool).read_bytes()[:16]);report[tool]['image_header_unvalidated']={'text':vals[1],'data':vals[2],'bss':vals[3],'total':sum(vals[1:4])}
# Measure the linker's table element on the target.
layout=work/'ld-layout';layout.mkdir(exist_ok=True)
ldsource=(pcc/'ldz8.c').read_text()
start=ldsource.index('typedef struct symbol *symp;')
end=ldsource.index('typedef struct arg_link', start)
(layout/'layout.c').write_text('#include "b.out.h"\n'+ldsource[start:end]+
                              'int sizes[] = {sizeof(struct symbol),sizeof(symp)};\n')
pre=subprocess.run(['cpp','-nostdinc','-undef','-I'+str(pcc),str(layout/'layout.c')],
                   capture_output=True,check=True)
compiled=subprocess.run([str(pcc/'cz8/cz8')],input=pre.stdout,capture_output=True,check=True)
(layout/'layout.az8').write_bytes(compiled.stdout)
subprocess.run([str(pcc/'az8/az8'),'-o','layout.b','layout.az8'],cwd=layout,check=True)
raw=(layout/'layout.b').read_bytes();h=struct.unpack('>8H',raw[:16])
symbol_size,pointer_size=struct.unpack('>2H',raw[16+h[1]:20+h[1]])
nsym=int(re.search(r'#define\s+NSYM\s+(\d+)',ldsource)[1])
nsympr=int(re.search(r'#define\s+NSYMPR\s+(\d+)',ldsource)[1])
block=int(re.search(r'#define\s+SYMBLOCK\s+(\d+)',ldsource)[1])
blocks=(nsym+block-1)//block
report['ldz8']['table_layout']={'symbol_bytes':symbol_size,'pointer_bytes':pointer_size,
    'symbol_limit':nsym,'dynamic_block_bytes':symbol_size*block,
    'symblocks':pointer_size*blocks,'hshtab':pointer_size*(nsym+2),'local':pointer_size*nsympr,
    'static_total':pointer_size*(blocks+nsym+2+nsympr)}

# Only cpp linked successfully in the initial audit. Never execute a failed
# link or an image whose static footprint already exhausts the address space.
cpp_result=report['cpp']
if cpp_result.get('link_returncode')==0 and cpp_result['image_header_unvalidated']['total']<65536:
 (work/'probe.h').write_text('#define BASE 40\n#define ADD(a,b) ((a)+(b))\n')
 (work/'probe.c').write_text('#include "probe.h"\n#if BASE == 40\n'
     'int native_cpp_probe = ADD(BASE,2);\n#else\nWRONG_BRANCH\n#endif\n')
 proto=f'''boot
800 64
d--755 0 0
bin d--755 0 0
 sh ---755 0 0 {root}/tools/sh
 cat ---755 0 0 {root}/tools/cat
 cpp ---755 0 0 {work}/cpp/cpp
 $
dev d--755 0 0
 console c--644 0 0 0 0
 tty c--644 0 0 2 0
 $
etc d--755 0 0
 init ---755 0 0 {root}/tools/init
 $
tmp d--777 0 0
 probe.c ---644 0 0 {work}/probe.c
 probe.h ---644 0 0 {work}/probe.h
 $
$
'''
 (work/'proto').write_text(proto)
 subprocess.run([str(root/'tools/v7mkfs'),str(work/'hd.img'),str(work/'proto')],check=True)
 build=root/'v7z8000/usr/sys/build'
 run=subprocess.run([str(build/'test_driver'),'-c','400000000','-d',str(work/'hd.img'),
     '-i','cpp -P /tmp/probe.c /tmp/probe.i\\ncat /tmp/probe.i\\n',
     '-w','int native_cpp_probe =  ((40)+(2));','-I','exit\\n',
     '-x','int native_cpp_probe =  ((40)+(2));'],cwd=build,capture_output=True,timeout=60)
 (work/'cpp-smoke.log').write_bytes(run.stdout+run.stderr)
 cpp_result['native_smoke_passed']=run.returncode==0 and b'WRONG_BRANCH' not in run.stdout
(work/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:{a:b for a,b in v.items() if a!='files'} for k,v in report.items()},indent=2))

if cpp_result.get("native_smoke_passed") is False:
 raise SystemExit("Native cpp smoke test failed; see cpp-smoke.log")
