/* Z8000 instruction display. Sole encoding authority: CPU Technical Manual,
 * chapter 6 (including EPA templates) and Appendix C. No instruction executes.
 * adbseg selects address encoding; it does not change adb's memory maps.
 */
#include "defs.h"
L_INT dot;
INT dotinc;
INT adbseg;
static int ispace, ibytes;
static char *conditions[]={"f","lt","le","ule","ov","mi","eq","c",
 "t","ge","gt","ugt","nov","pl","ne","nc"};
static char *binary[]={"add","sub","or","and","xor","cp"};
static char *longops[]={"cpl","","subl","","ldl","","addl","","multl","mult","divl","div"};
static char *unary[]={"com","cp","neg","","test","ld","tset","","clr","push"};
static char *bitops[]={"res","set","bit","inc","dec"};
static char *ctl[]={"fcw","refresh","psapseg","psapoff","nspseg","nspoff"};

nextword()
{unsigned w;w=chkget(inkdot(ibytes),ispace);ibytes+=2;return(w);}
reg(n,kind)
{
 if(kind==0)printf("r%d",n);
 else if(kind==1)printf("%s%d",n<8?"rh":"rl",n&7);
 else printf("%s%d",kind==2?"rr":"rq",n);
}
hexword(w) unsigned w;
{int n;char *digits="0123456789abcdef";for(n=12;n>=0;n-=4)printc(digits[(w>>n)&15]);}
imm(w) unsigned w; {printc('#');hexword(w);}
address(w,index,type)
unsigned w;
{w&=65535;psymoff(leng(w),type,"");if(index)printf("(r%d)",index);}
/* Address operands use register pairs in SEG; I/O port registers do not. */
areg(n) {reg(n,adbseg?2:0);}
indirect(n) {printc('@');areg(n);}
memaddr(index,type)
{
 unsigned w,seg,off;
 w=nextword();
 if(!adbseg)address(w,index,type);
 else {
  seg=(w>>8)&127;off=(w&0x8000)?(unsigned)nextword():w&255;
  hexword(seg);printc(':');hexword(off);
  if(index)printf("(r%d)",index);
 }
}
operand(mode,r,kind,type)
{
 if(mode==2)reg(r,kind);
 else if(mode==1)memaddr(r,type);
 else indirect(r);
}
mnemonic(name,byte)
char *name;
{prints(name);if(byte)printc('b');printc('\t');}
comma() {printc(',');}
flags(n)
{int i,first;char *names="czsv";first=1;for(i=0;i<4;i++)if(n&(8>>i)) {
 if(!first)comma();printc(names[i]);first=0;
 }if(first)prints("#0");}
interrupts(n)
{if(n&2)prints("vi");if(n==3)comma();if(n&1)prints("nvi");if(!n)prints("#0");}
/* These words contain CPU-defined transfer information and EPU-defined
 * operation bits. Keep the latter visible rather than invent an EPU mnemonic.
 */
epa(h,a,b)
{
 unsigned w;
 if((h==0x0f && a || h==0x4f) && (b&7)>=4) {
  w=nextword();prints("epu\t");
  if(b&8) {operand(h==0x4f?1:0,a,0,DSYM);prints(",epu");}
  else {prints("epu,");operand(h==0x4f?1:0,a,0,DSYM);}
  printf(",#%d,",(w&255)+1);imm(w);printf(",#%d",b&3);return(1);
 }
 if(h==0x8f && !(a&8) && !(b&4)) {
  w=nextword();prints("epu\t");
  if(b&8) {prints("epu,");reg((w>>8)&15,0);}
  else {reg((w>>8)&15,0);prints(",epu");}
  printf(",#%d,",(w&15)+1);imm(w);comma();imm((a<<4)|b);return(1);
 }
 if(h==0x8e && (b&12)!=12) {
  w=nextword();prints("epu\t");
  prints((b&12)==0?"flags,epu":((b&12)==8?"epu,flags":"internal"));
  comma();imm(w);comma();imm((a<<4)|b);return(1);
 }
 return(0);
}

printins(f,space,word)
unsigned word;
{
 unsigned h,a,b,w,c,r;
 int mode,byte,kind,op,disp,pop,back,repeat;
 char *name;
 ispace=space;ibytes=2;h=word>>8;a=(word>>4)&15;b=word&15;
 mode=h>=128?2:(h>=64?1:0);byte=!(h&1);
 if((h<=0x0b)||(h>=0x40&&h<=0x4b)||(h>=0x80&&h<=0x8b)) {
  mnemonic(binary[(h&15)>>1],byte);reg(b,byte);comma();
  if(mode==0&&!a) {w=nextword();imm(byte?w&255:w);}
  else operand(mode,a,byte,DSYM);
 } else if((h&0xf0)==0xc0) {
  mnemonic("ld",1);reg(h&15,1);comma();imm(word&255);
 } else if((h&0xf0)==0xe0) {
  prints("jr\t");prints(conditions[h&15]);comma();
  disp=word&255;if(disp&128)disp-=256;
  address((unsigned)(dot+2+2*disp),0,ISYM);
 } else if((h&0xf0)==0xd0) {
  /* Preserve the signed encoded field: historical CALR dialects disagree
   * with chapter 6's target calculation. Instruction length is unambiguous. */
  disp=word&4095;if(disp&2048)disp-=4096;
  printf("calr\tdisp %d",disp);
 } else if((h&0xf0)==0xf0) {
  mnemonic((word&128)?"djnz":"dbjnz",0);reg(h&15,!(word&128));comma();
  address((unsigned)(dot+2-2*(word&127)),0,ISYM);
 } else if(h==0x7f) {prints("sc\t");imm(word&255);
 } else if((word&0xfff0)==0x9e00) {
  prints("ret\t");prints(conditions[b]);
 } else if((h==0x1f&&a || h==0x5f)&&!b) {
  prints("call\t");operand(mode,a,0,ISYM);
 } else if(h==0x1e&&a || h==0x5e) {
  prints("jp\t");prints(conditions[b]);comma();operand(mode,a,0,ISYM);
 } else if(h==0x20||h==0x21||h==0x60||h==0x61||h==0xa0||h==0xa1) {
  mnemonic("ld",byte);reg(b,byte);comma();
  if(!mode&&!a) {w=nextword();imm(byte?w&255:w);}else operand(mode,a,byte,DSYM);
 } else if((h==0x2e||h==0x2f)&&a || h==0x6e||h==0x6f || h==0x1d&&a || h==0x5d) {
  kind=h==0x1d||h==0x5d?2:byte;
  mnemonic(kind==2?"ldl":"ld",kind==1);operand(mode,a,0,DSYM);comma();reg(b,kind);
 } else if(h>=0x30&&h<=0x37) {
  kind=h==0x35||h==0x37?2:byte;
  if(h==0x34) {
   prints(a?"lda\t":"ldar\t");areg(b);comma();w=nextword();
   if(a) {areg(a);prints("(#");hexword(w);printc(')');}
   else address((unsigned)(dot+4+w),0,ISYM);
  } else if(h!=0x36) {
   pop=h==0x32||h==0x33||h==0x37;
   mnemonic(a?(kind==2?"ldl":"ld"):(kind==2?"ldrl":"ldr"),kind==1);
   w=nextword();if(!pop) {reg(b,kind);comma();}
   if(a) {areg(a);prints("(#");hexword(w);printc(')');}
   else address((unsigned)(dot+4+w),0,ISYM);
   if(pop) {comma();reg(b,kind);}
  } else goto raw;
 } else if((h>=0x70&&h<=0x75||h==0x77)&&a) {
  kind=h==0x75||h==0x77?2:byte;w=nextword();
  if(w&0xf0ff)goto raw;
  pop=h==0x72||h==0x73||h==0x77;
  mnemonic(h==0x74?"lda":(kind==2?"ldl":"ld"),h!=0x74&&kind==1);
  if(!pop) {if(h==0x74)areg(b);else reg(b,kind);comma();}
  areg(a);printc('(');reg((w>>8)&15,0);printc(')');
  if(pop) {comma();reg(b,kind);}
 } else if(h==0x76) {
  prints("lda\t");areg(b);comma();memaddr(a,DSYM);
 } else if((h==0x93||h==0x91||h==0x97||h==0x95)&&a) {
  kind=h==0x91||h==0x95?2:0;pop=h==0x97||h==0x95;
  mnemonic(kind?(pop?"popl":"pushl"):(pop?"pop":"push"),0);
  if(pop) {reg(b,kind);comma();indirect(a);}
  else {indirect(a);comma();reg(b,kind);}
 } else if((h==0x11||h==0x13||h==0x15||h==0x17)&&a&&b) {
  pop=h==0x15||h==0x17;
  mnemonic(h==0x11||h==0x15?(pop?"popl":"pushl"):(pop?"pop":"push"),0);
  indirect(pop?b:a);comma();indirect(pop?a:b);
 } else if((h==0x51||h==0x53||h==0x55||h==0x57)&&a) {
  pop=h==0x55||h==0x57;
  mnemonic(h==0x51||h==0x55?(pop?"popl":"pushl"):(pop?"pop":"push"),0);
  if(!pop) {indirect(a);comma();}
  memaddr(b,DSYM);if(pop) {comma();indirect(a);}
 } else if((h==0x1c&&a || h==0x5c)&&(b==1||b==9)) {
  w=nextword();if(w&0xf0f0)goto raw;prints("ldm\t");
  if(b==1) {reg((w>>8)&15,0);comma();}
  operand(mode,a,0,DSYM);
  if(b==9) {comma();reg((w>>8)&15,0);}
  printf(",#%d",(w&15)+1);
 } else if((h==0x1c&&a || h==0x5c||h==0x9c)&&b==8) {
  prints("testl\t");operand(mode,a,2,DSYM);
 } else if((h==0x0c||h==0x0d)&&a || h==0x4c||h==0x4d||h==0x8c||h==0x8d) {
  if(h==0x8c&&(b==1||b==9)) {
   prints("ldctlb\t");if(b==9)prints("flags,");reg(a,1);if(b==1)prints(",flags");
  } else if(h==0x8d&&(b==1||b==3||b==5)) {
   mnemonic(b==1?"setflg":(b==3?"resflg":"comflg"),0);flags(a);
  } else if(word==0x8d07)prints("nop");
  else if(b<10 && unary[b][0] && !(mode==2&&(b==1||b==5||b==9)) &&
          !(mode==1&&b==9) && !(h==0x0c&&b==9)) {
   mnemonic(unary[b],byte);operand(mode,a,byte,DSYM);
   if(b==1||b==5||b==9) {comma();w=nextword();imm(byte?w&255:w);}
  } else goto raw;
 } else if((h>=0x22&&h<=0x2b)||(h>=0x62&&h<=0x6b)||(h>=0xa2&&h<=0xab)) {
  op=((h&15)-2)>>1;
  if(!mode&&!a&&op<3) {
   w=nextword();if(w&0xf0ff)goto raw;
   mnemonic(bitops[op],byte);reg((w>>8)&15,byte);comma();reg(b,0);
  } else if(!mode&&!a)goto raw;
  else {mnemonic(bitops[op],byte);operand(mode,a,byte,DSYM);comma();imm(op>=3?b+1:b);}
 } else if((h>=0x10&&h<=0x1b)||(h>=0x50&&h<=0x5b)||(h>=0x90&&h<=0x9b)) {
  op=h&15;if(!longops[op][0])goto raw;
  mnemonic(longops[op],0);kind=op==8||op==10?3:2;
  reg(b,kind);comma();
  if(!mode&&!a) {w=nextword();imm(w);if(op!=9&&op!=11)hexword((unsigned)nextword());}
  else operand(mode,a,op==9||op==11?0:2,DSYM);
 } else if((h==0x2c||h==0x2d)&&a || h==0x6c||h==0x6d||h==0xac||h==0xad) {
  mnemonic("ex",byte);reg(b,byte);comma();operand(mode,a,byte,DSYM);
 } else if(h==0xae||h==0xaf) {
  mnemonic("tcc",byte);prints(conditions[b]);comma();reg(a,byte);
 } else if(h==0xb0&&!b) {prints("dab\t");reg(a,1);
 } else if(h==0xb1&&(b==0||b==7||b==10)) {
  mnemonic(b==0?"extsb":(b==7?"extsl":"exts"),0);reg(a,b==0?0:(b==7?3:2));
 } else if(h>=0xb4&&h<=0xb7) {
  mnemonic(h<0xb6?"adc":"sbc",byte);reg(b,byte);comma();reg(a,byte);
 } else if(h==0xb2||h==0xb3) {
  if(!(b&1)) {
   name=(b&8)?((b&4)?"rrc":"rlc"):((b&4)?"rr":"rl");
   mnemonic(name,byte);reg(a,byte);printf(",#%d",(b&2)?2:1);
  } else if(byte&&(b&4))goto raw;
  else {
   kind=b&4?2:byte;w=nextword();
   if(b&2) {
    if(w&0xf0ff)goto raw;
    name=b&8?"sda":"sdl";prints(name);
    if(kind==2)printc('l');else if(kind==1)printc('b');printc('\t');
    reg(a,kind);comma();reg((w>>8)&15,0);
   } else {
    disp=byte?(int)(char)(w&255):(int)(short)w;
    /* char is unsigned on the target: sign-extend the byte explicitly. */
    if(byte) {disp=w&255;if(disp&128)disp-=256;}
    name=b&8?(disp<0?"sra":"sla"):(disp<0?"srl":"sll");
    prints(name);if(kind==2)printc('l');else if(kind==1)printc('b');printc('\t');
    reg(a,kind);printf(",#%d",disp<0?-disp:disp);
   }
  }
 } else if(h==0xbc||h==0xbe) {
  mnemonic(h==0xbc?"rrdb":"rldb",0);reg(b,1);comma();reg(a,1);
 } else if(h==0xbd) {prints("ldk\t");reg(a,0);comma();imm(b);
 } else if(h==0xb8&&a&&!(b&1)) {
  w=nextword();
  if((w&0xf000) || (w&15)!=((b==6||b==14)?14:0) || !((w>>4)&15))goto raw;
  /* Ch.6 TRT repeat formats carry 1110 instead of 0000 in the low nibble. */
  name=b==0?"trib":(b==2?"trtib":(b==4?"trirb":(b==6?"trtirb":
       (b==8?"trdb":(b==10?"trtdb":(b==12?"trdrb":"trtdrb"))))));
  mnemonic(name,0);indirect(a);comma();indirect((w>>4)&15);comma();reg((w>>8)&15,0);
 } else if((h==0xba||h==0xbb)&&a&&(b==1||b==9||!(b&1))) {
  w=nextword();if(w&0xf000)goto raw;c=w&15;r=(w>>4)&15;
  if(b==1||b==9) {
   if(!r || (c!=0&&c!=8))goto raw;
   name=b==1?(c?"ldi":"ldir"):(c?"ldd":"lddr");
   mnemonic(name,byte);indirect(r);comma();indirect(a);comma();reg((w>>8)&15,0);
  } else {
   if(b&2) {if(!r)goto raw;}
   name=b&8?(b&4?(b&2?"cpsdr":"cpdr"):(b&2?"cpsd":"cpd")):
                   (b&4?(b&2?"cpsir":"cpir"):(b&2?"cpsi":"cpi"));
   mnemonic(name,byte);if(b&2)indirect(r);else reg(r,byte);
   comma();indirect(a);comma();reg((w>>8)&15,0);comma();prints(conditions[c]);
  }
 } else if((h==0x3c||h==0x3d||h==0x3e||h==0x3f)&&a) {
  pop=h>=0x3e;mnemonic(pop?"out":"in",byte);
  if(!pop) {reg(b,byte);prints(",@");reg(a,0);}
  else {printc('@');reg(a,0);comma();reg(b,byte);}
 } else if(h==0x3a||h==0x3b) {
  if(b>=4&&b<=7) {
   w=nextword();if(b&1)printc('s');mnemonic(b&2?"out":"in",byte);
   if(!(b&2)) {reg(a,byte);comma();}imm(w);
   if(b&2) {comma();reg(a,byte);}
  } else if(a&&(b==0||b==1||b==2||b==3||b==8||b==9||b==10||b==11)) {
   w=nextword();r=(w>>4)&15;c=w&15;
   if(w&0xf000 || !r || (c!=0&&c!=8))goto raw;
   back=b&8;repeat=!c;pop=b&2;
   if(b&1)printc('s');
   name=pop?(back?(repeat?"otdr":"outd"):(repeat?"otir":"outi")):
            (back?(repeat?"indr":"ind"):(repeat?"inir":"ini"));
   mnemonic(name,byte);
   if(pop) {printc('@');reg(r,0);comma();indirect(a);}
   else {indirect(r);prints(",@");reg(a,0);}
   comma();reg((w>>8)&15,0);
  } else goto raw;
 } else if((h==0x39&&a || h==0x79)&&!b) {
  prints("ldps\t");operand(mode,a,0,DSYM);
 } else if(h==0x7c && !a && b<8) {
  mnemonic(b&4?"ei":"di",0);interrupts(b&3);
 } else if(h==0x7d && b>=2&&b<=15 && b!=8&&b!=9 &&
           (adbseg || b!=4&&b!=6&&b!=12&&b!=14)) {
  op=(b&7)-2;prints("ldctl\t");
  name=ctl[op];if(!adbseg&&op==3)name="psap";if(!adbseg&&op==5)name="nsp";
  if(b&8) {prints(name);comma();reg(a,0);}
  else {reg(a,0);comma();prints(name);}
 } else if(word==0x7a00)prints("halt");
 else if(word==0x7b00)prints("iret");
 else if(word==0x7b08)prints("mset");
 else if(word==0x7b09)prints("mres");
 else if(word==0x7b0a)prints("mbit");
 else if(h==0x7b&&b==13) {prints("mreq\t");reg(a,0);}
 else if(epa(h,a,b)) ;
 else {
raw: prints(".word\t");imm(word);
  for(op=2;op<ibytes;op+=2) {comma();imm((unsigned)chkget(inkdot(op),space));}
 }
 dotinc=ibytes;return;
}
