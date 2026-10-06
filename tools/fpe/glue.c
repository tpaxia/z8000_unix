/* PCC ABI glue for the system's Zilog EPU emulator. No software arithmetic
 * engine is linked here: the assembly primitives execute EPA instructions.
 */
static copy4(d,s)
unsigned short *d,*s;
{ int i; for (i=0;i<4;i++) d[i]=s[i]; }

static nan(a)
unsigned short *a;
{ return (a[0]&0x7ff0)==0x7ff0 && ((a[0]&15)||a[1]||a[2]||a[3]); }

/* Retain the existing PCC ABI's NaN payload/conversion convention. The
 * historical EPU's native payload layout differs; finite arithmetic stays
 * entirely in the service. */
daddcore(out,a,b)
unsigned short *out,*a,*b;
{
	if(nan(a)) { copy4(out,a);out[0]|=8; }
	else if(nan(b)) { copy4(out,b);out[0]|=8; }
	else fpeadd(out,a,b);
}

dsubcore(out,a,b)
unsigned short *out,*a,*b;
{ unsigned short t[4];copy4(t,b);t[0]^=32768;daddcore(out,a,t); }

static nanpair(out,a,b)
unsigned short *out,*a,*b;
{
	if(!nan(a) && !nan(b)) return 0;
	out[0]=0x7ff8;out[1]=out[2]=out[3]=0;return 1;
}

dmulcore(out,a,b)
unsigned short *out,*a,*b;
{ if(!nanpair(out,a,b)) fpemul(out,a,b); }

ddivcore(out,a,b)
unsigned short *out,*a,*b;
{ if(!nanpair(out,a,b)) fpediv(out,a,b); }

ftodcore(out,a)
unsigned short *out,*a;
{
	if((a[0]&0x7f80)==0x7f80 && ((a[0]&127)||a[1])) {
		out[0]=(a[0]&32768)|0x7ff8;out[1]=out[2]=out[3]=0;
	} else fpeftod(out,a);
}

dtofcore(out,a)
unsigned short *out,*a;
{
	if(nan(a)) { out[0]=(a[0]&32768)|0x7fc0;out[1]=0; }
	else fpedtof(out,a);
}

dnegcore(d,s)
unsigned short *d,*s;
{ copy4(d,s); d[0]^=32768; }

dcompare(a,b,op)
unsigned short *a,*b;
int op;
{
	int f;
	f=fpcmp(a,b);
	if(f&16) return op==1;
	switch(op) {
	case 0:return (f&64)!=0;
	case 1:return (f&64)==0;
	case 2:return (f&128)!=0;
	case 3:return (f&192)!=0;
	case 4:return (f&192)==0;
	case 5:return (f&128)==0;
	}
	return 0;
}

itodcore(out,a,width,uns)
unsigned short *out,*a;
int width,uns;
{
	unsigned short v[4];
	v[3]=a[width-1];
	v[2]=width==2?a[0]:((!uns && (a[0]&32768))?65535:0);
	v[0]=v[1]=(!uns && (a[0]&32768))?65535:0;
	fpi64(out,v);
}

dtoicore(out,a,width,uns)
unsigned short *out,*a;
int width,uns;
{
	unsigned short v[4];
	int e;
	e=((a[0]>>4)&2047)-1023;
	if(e<0 || e>=width*16) {
		v[2]=v[3]=0;
	} else fpto64(v,a);
	out[0]=v[4-width];
	if(width==2) out[1]=v[3];
}

dfopcore(out,a,b,op,single)
unsigned short *out,*a,*b;
int op,single;
{
	unsigned short x[4],y[4],z[4];
	if(single) { ftodcore(x,a); ftodcore(y,b); }
	else { copy4(x,a); copy4(y,b); }
	switch(op) {
	case 0:daddcore(z,x,y);break;
	case 1:dsubcore(z,x,y);break;
	case 2:dmulcore(z,x,y);break;
	case 3:ddivcore(z,x,y);break;
	}
	if(single) dtofcore(out,z);else copy4(out,z);
}

dafcore(out,a,b,op,single)
unsigned short *out,*a,*b;
int op,single;
{
	unsigned short x[4],z[4];
	if(single) { ftodcore(x,a);dfopcore(z,x,b,op,0);dtofcore(out,z); }
	else dfopcore(out,a,b,op,0);
	if(single) { a[0]=out[0];a[1]=out[1]; }else copy4(a,out);
}

postcore(out,a,delta,single)
unsigned short *out,*a;
int delta,single;
{
	unsigned short x[4],one[4],z[4];
	if(single) { out[0]=a[0];out[1]=a[1];ftodcore(x,a); }
	else { copy4(out,a);copy4(x,a); }
	one[0]=delta<0?0xbff0:0x3ff0;one[1]=one[2]=one[3]=0;
	daddcore(z,x,one);
	if(single) dtofcore(a,z);else copy4(a,z);
}
