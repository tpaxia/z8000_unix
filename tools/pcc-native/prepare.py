from pathlib import Path
import re,subprocess,sys,shutil
root=Path(__file__).resolve().parents[2]
w=Path(sys.argv[1]).resolve()
w.mkdir(parents=True,exist_ok=True)
src=root/'PCC-z8000/z8000/cz8'
for source in src.iterdir():
 if source.suffix=='.c' or source.name in ['manifest','macdefs','mac2defs','mfile1','mfile2','common']:
  shutil.copyfile(source,w/source.name)
(w/'macdefs').write_text((w/'macdefs').read_text().replace('# define ONEPASS','/* separate passes */'))
# Stage the two-pass sources used by the native build and self-hosting trial.
s=(src/'local.c').read_text().replace('p2tree( p );\n\tp2compile( p );','printf("@%d\\t%s\\n", lineno, ftitle);\n\tprtree(p);')
(w/'local.c').write_text(s)
s=(src/'trees.c').read_text().replace('else sprintf( p->in.name, LABFMT, -p->tn.rval );','else sprintf( p->in.name, LABFMT, -p->tn.rval );')
s=s.replace('printf( LABFMT, -p->tn.rval );','printf( LABFMT, -p->tn.rval );\n\t\t\tputchar(\'\\n\');')
(w/'trees.c').write_text(s)
local2=(src/'local2.c').read_text()
names=local2[local2.index('char *\nrnames[]'):local2.index('int rstatus[]')]
szty=local2[local2.index('szty(t) TWORD'):local2.index('\n}',local2.index('szty(t) TWORD'))+2]
(w/'frontglue.c').write_text('''#include "mfile1"
int usedregs;
'''+names+szty+'''
p2bbeg(aoff, reg) { printf("[%d\\t%d\\t%d\\t%d\\t\\n", ftnno,aoff,reg,usedregs); }
p2bend() { printf("]%d\\t\\n",retlab); }
''')
(w/'comm2.c').write_text('''#include "mfile2"
#undef EXIT
#define EXIT exit
#include "common"
''')
(w/'backglue.c').write_text('''#include "mfile2"
int retlab;
short revrel[] = { EQ, NE, GE, GT, LE, LT, UGE, UGT, ULE, ULT };
where(c) { fprintf(stderr,"%s, line %d: ",filename,lineno); }
NODE *block(o,l,r,t,d,s) NODE *l,*r; TWORD t; {
 NODE *p;
 p=talloc();
 p->in.op=o; p->in.left=l; p->in.right=r; p->in.type=t;
 p->in.rall=NOPREF; p->in.name[0]=0;
 return p;
}
NODE *makety(p,t,d,s) NODE *p; TWORD t; {
 if(p->in.type==t) return p;
 return block(SCONV,p,NIL,t,0,0);
}
''')
s=(src/'reader.c').read_text().replace('register NODE *p;\n\n\tfiles = p2init','register NODE *p;\n\tint incoming_regs;\n\textern int usedregs, retlab;\n\n\tfiles = p2init',1)
s=s.replace('maxtreg = rdin(10);','maxtreg = rdin(10);\n\t\tincoming_regs = rdin(10);',1)
s=s.replace('maxoff = baseoff;\n\t\t\tftnno = temp;', 'maxoff = baseoff;\n\t\t\tusedregs = incoming_regs;\n\t\t\tftnno = temp;',1)
s=s.replace("case ']':  /* end of block */", "case ']':  /* end of block */\n\t\tretlab = rdin(10);")
s=s.replace("case '.':", "case '@':",1)
s=s.replace('cerror( "intermediate file format error" );\n\n\t\t}', 'do { PUTCHAR(c); } while(c!=\'\\n\' && (c=getchar())>0);\n\n\t\t}',1)
(w/'reader.c').write_text(s)
flags=['-O','-w','-Wno-implicit-int','-Wno-implicit-function-declaration','-Wno-return-mismatch','-Wno-return-type','-Wno-int-conversion']
for name,objs in [('front','cgram xdefs scan pftn trees optim code local comm1 frontglue'),('back','reader local2 order match allo comm2 table backglue')]:
 r=subprocess.run(['cc',*flags,*[x+'.c' for x in objs.split()],'-o',name],cwd=w,capture_output=True)
 (w/(name+'-connected.log')).write_bytes(r.stdout+r.stderr)
 print(name,r.returncode,r.stderr.decode()[-3000:])
 r.check_returncode()
