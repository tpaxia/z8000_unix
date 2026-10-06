/* Appended to the actual kernel passc/cpass/iomove sources by test-copy.py.
 * Run with the real target ABI and user structure, substituting only the
 * machine helpers. Distinct arrays model kernel, user D and user I memory.
 */
union { int align; char bytes[16]; } kmem, dmem, imem, buffer;
int calls, moved, fault, space, failed, cases;

bytecopy(addr, value, writing, instruction)
char *addr;
{
	int index;
	char *mem;
	calls++;
	if (instruction != (space == 2)) failed++;
	index = addr - kmem.bytes;
	if (index < 0 || index >= 16 || (fault >= 0 && moved == fault))
		return(-1);
	mem = instruction ? imem.bytes : dmem.bytes;
	moved++;
	if (writing) { mem[index] = value; return(0); }
	return(mem[index] & 0377);
}
fubyte(p) char *p; { return(bytecopy(p, 0, 0, 0)); }
fuibyte(p) char *p; { return(bytecopy(p, 0, 0, 1)); }
subyte(p, c) char *p; { return(bytecopy(p, c, 1, 0)); }
suibyte(p, c) char *p; { return(bytecopy(p, c, 1, 1)); }

bulk(from, to, n, writing, instruction)
char *from, *to;
{
	int c;
	while (n--) {
		if (writing) {
			if (bytecopy(to++, *from++, 1, instruction) < 0) return(-1);
		} else {
			c = bytecopy(from++, 0, 0, instruction);
			if (c < 0) return(-1);
			*to++ = c;
		}
	}
	return(0);
}
copyin(f,t,n) char *f,*t; { return(bulk(f,t,n,0,0)); }
copyiin(f,t,n) char *f,*t; { return(bulk(f,t,n,0,1)); }
copyout(f,t,n) char *f,*t; { return(bulk(f,t,n,1,0)); }
copyiout(f,t,n) char *f,*t; { return(bulk(f,t,n,1,1)); }

trial(seg, direction, ua, ka, n, stop)
{
	int i, done, accounted, err, fast, before;
	before = failed;
	space = seg; fault = stop; moved = calls = 0;
	for (i=0; i<16; i++) {
		kmem.bytes[i] = 0x80+i;
		dmem.bytes[i] = 0xa0+i;
		imem.bytes[i] = 0xe0+i;
		buffer.bytes[i] = 0xc0+i;
	}
	u.u_segflg = seg; u.u_error = 0;
	u.u_base = kmem.bytes+ua; u.u_count = n+3; u.u_offset = 65534L;
	iomove(buffer.bytes+ka, n, direction);
	done = n;
	if (seg != 1 && stop >= 0 && stop < n) done = stop;
	err = done != n;
	fast = seg != 1 && !(n&1) && !(ua&1) && !(ka&1);
	accounted = err && fast ? 0 : done;
	if (u.u_base != kmem.bytes+ua+accounted ||
	    u.u_count != n+3-accounted || u.u_offset != 65534L+accounted ||
	    u.u_error != (err ? EFAULT : 0) ||
	    ((seg == 1 || n == 0) && calls != 0)) failed++;
	for (i=0; i<16; i++) {
		int k,d,t,b,j;
		k=0x80+i; d=0xa0+i; t=0xe0+i; b=0xc0+i;
		if (direction == B_READ && i>=ua && i<ua+done) {
			j=0xc0+ka+i-ua;
			if(seg==1) k=j; else if(seg==0) d=j; else t=j;
		}
		if (direction == B_WRITE && i>=ka && i<ka+done)
			b=(seg==1 ? 0x80 : seg==0 ? 0xa0 : 0xe0)+ua+i-ka;
		if ((kmem.bytes[i]&0377)!=k || (dmem.bytes[i]&0377)!=d ||
		    (imem.bytes[i]&0377)!=t || (buffer.bytes[i]&0377)!=b) failed++;
	}
	cases++;
	if (before != failed)
		printf("copy: FAIL seg=%d dir=%d ua=%d ka=%d n=%d fault=%d\n",
		    seg,direction,ua,ka,n,stop);
}

main()
{
	int s,d,a,b,n,f,r;
	for(s=0;s<3;s++) for(d=0;d<2;d++)
	for(a=0;a<2;a++) for(b=0;b<2;b++) for(n=0;n<6;n++)
	for(f= -1;f<=n;f++) trial(s,d ? B_READ : B_WRITE,a,b,n,f);
	/* Stop at the end of the mock address space, after two valid bytes. */
	for(s=0;s<3;s+=2) for(d=0;d<2;d++) {
		space=s; fault= -1; calls=moved=0;
		u.u_segflg=s; u.u_error=0; u.u_base=kmem.bytes+14;
		u.u_count=3; u.u_offset=65534L;
		iomove(buffer.bytes,3,d ? B_READ : B_WRITE);
		if(u.u_error!=EFAULT || u.u_count!=1 ||
		    u.u_base!=kmem.bytes+16 || u.u_offset!=65536L) failed++;
		cases++;
	}
	for(s=0;s<3;s++) {
		space=s; fault= -1; calls=moved=0;
		u.u_segflg=s; u.u_error=0; u.u_base=kmem.bytes;
		u.u_count=1; u.u_offset=0;
		r=passc(255);
		if(r!=-1 || u.u_error || u.u_count || u.u_offset!=1) failed++;
		u.u_base=kmem.bytes; u.u_count=1;
		if(cpass()!=255 || u.u_count) failed++;
		r=calls;
		if(cpass()!=-1 || calls!=r || u.u_error) failed++;
		cases++;
	}
	printf("copy: %d cases, %d failures\n",cases,failed);
	return(failed!=0);
}
