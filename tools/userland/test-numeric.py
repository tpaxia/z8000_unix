#!/usr/bin/env python3
"""Install native-built factor/primes and compare guest output with integers."""
from pathlib import Path
import json
import math
import subprocess
import sys
sys.dont_write_bytecode = True
import test as packages
from object_format import sizes

ROOT, SYS = packages.ROOT, packages.SYS
NATIVE = ROOT/'tests/build/userland-native-sout'
WORK = ROOT/'tests/build/userland-numeric'
WORK.mkdir(parents=True, exist_ok=True)

def factors(n):
    answer=[]
    d=2
    while d*d<=n:
        while n%d==0:
            answer.append(d)
            n//=d
        d+=1
    if n>1:answer.append(n)
    return b'\n'+b''.join(('     %d\n'%p).encode() for p in answer)

def primes(start, count):
    answer=[]
    n=max(2,start)
    while len(answer)<count:
        if all(n%d for d in range(2,math.isqrt(n)+1)):
            answer.append(n)
        n+=1
    return b''.join(('%d\n'%p).encode() for p in answer)

expected={}
commands=['cd /tmp']
for i,n in enumerate([1,2,4,90,97,10201,10403,2147483647,65537**2,1<<55,(1<<56)-1]):
    name='factor%d'%i
    commands.append('factor %d > /tmp/%s'%(n,name))
    expected[name]=factors(n)
for i,(start,count) in enumerate([(1,1100),(97,25),(100,25),(8099,40),
                                (65529,30),(4294967290,10),(65537**2-10,10)]):
    name='primes%d'%i
    commands.append("primes %d | sed -n '1,%dp;%dq' > /tmp/%s"%(start,count,count,name))
    expected[name]=primes(start,count)
commands += [
    '(echo 90; echo 1; echo 0; echo 77) | factor > /tmp/stream',
    'factor " 1 2 " > /tmp/spaces',
    '(echo 72057594037927936; echo 90; echo 0) | factor > /tmp/retry 2> /tmp/ouch',
    'factor 72057594037927936 > /tmp/overflow 2> /tmp/argerr',
    "echo 100 | primes | sed -n '1,25p;25q' > /tmp/stdinprimes",
    'primes 72057594037927935 > /tmp/end',
    'primes 72057594037927936 > /tmp/poverflow 2> /tmp/perr',
    'factor 0 > /tmp/zero',
    'factor garbage > /tmp/garbage',
    'sync', "echo NUMERIC-''DONE",
]
expected.update(stream=factors(90)+factors(1),spaces=factors(12),retry=factors(90),
    ouch=b'Ouch.\n',overflow=b'',argerr=b'Ouch.\n',stdinprimes=primes(100,25),
    end=b'',poverflow=b'',perr=b'Ouch.\n',zero=b'',garbage=b'')
def trial(name, source, commands, verdict):
    actions=WORK/(name+'-actions')
    actions.write_text(''.join('# \t'+cmd+'\\n\n' for cmd in commands))
    log=WORK/(name+'.log')
    saved=WORK/(name+'.img')
    with log.open('wb') as out:
        result=subprocess.run([str(SYS/'test_driver'),'-T','66667',
            '-d',str(source),'-i','','-q','# ','-A',str(actions),
            '-c','100000000000','-x',verdict,'-o',str(saved)],
            cwd=SYS,stdout=out,stderr=subprocess.STDOUT,timeout=900)
    assert result.returncode==0, log
    output=log.read_bytes()
    assert b'Absent RAM accesses: 0' in output and b'panic:' not in output,log
    return saved,output,log

installed,output,log=trial('install',NATIVE/'hd.img',[
    'cd /usr/src/build/factor; make install; echo install-factor $?: done',
    'cd /usr/src/build/primes; make install; echo install-primes $?: done',
    'sync',"echo NUMERIC-''INSTALLED"], 'NUMERIC-INSTALLED')
for name in ('factor','primes'):
    assert ('install-'+name+' 0: done\r\n').encode() in output,log
# V7 make grows its user stack during installation. Check the programs
# separately so their run has its own MMU counters and fresh boot.
saved,output,log=trial('guest',installed,commands,'NUMERIC-DONE')
assert b'Absent RAM accesses: 0' in output and b'Unmapped accesses: 0' in output, log
fs=packages.Filesystem(saved)
for name,wanted in expected.items():
    actual=fs.read('/tmp/'+name)
    assert actual==wanted,(name,actual[:300],wanted[:300],log)
for name in ('factor','primes'):
    program=fs.read('/bin/'+name)
    assert program==fs.read('/usr/src/build/'+name+'/'+name)
    assert program[:2]==b'\xe7\x11'
    dest=WORK/name
    dest.write_bytes(program)
    print('PASS native',name,sizes(program),flush=True)
saved.replace(NATIVE/'hd.img')
(WORK/'expected.json').write_text(json.dumps({n:v.decode() for n,v in expected.items()},indent=2)+'\n')
print('PASS factor/primes: exact 56-bit range, repeated factors, streaming input, sieve boundaries and 32-bit crossing',flush=True)
