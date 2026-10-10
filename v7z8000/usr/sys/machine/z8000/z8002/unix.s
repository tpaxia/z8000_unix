! Banked Z8002 EPU service. The kernel data map and stack stay installed.
! Entry/exit gates in kernel I page zero switch only instruction context.
.unsegm
.text
.global epu
fpe_run:
 ld userd,r10
 ld useri,r11
 clr 136(r13)
 call epu
 ld r0,r2
 jp 0x0190

.global gettext,getmem,putmem
gettext:
 push @sp,r7
 ld r6,useri
 call rdbyte
 test r2
 jr nz,textdone
 ldb rh3,rl1
 inc r7,#1
 call rdbyte
 test r2
 jr nz,textdone
 ldb rl3,rl1
textdone:
 pop r7,@sp
 ret
getmem:
 cp r3,#20
 jp ugt,memfail
 test r3
 jr z,memdone
 ld r0,r7
 add r0,r3
 jr nc,getvalid
 test r0
 jp nz,memfail
getvalid:
 ld r6,userd
getloop:
 call rdbyte
 test r2
 ret nz
 ldb @r5,rl1
 inc r7,#1
 inc r5,#1
 djnz r3,getloop
memdone:
 clr r2
 ret
putmem:
 cp r3,#20
 jp ugt,memfail
 test r3
 jr z,memdone
 ld r0,r5
 add r0,r3
 jr nc,putvalid
 test r0
 jp nz,memfail
putvalid:
 push @sp,r7
 push @sp,r6
 ld r4,r7
 ld r6,userd
 ld r7,r5
putloop:
 ldb rl1,@r4
 call wrbyte
 test r2
 jr nz,putdone
 inc r7,#1
 inc r4,#1
 djnz r3,putloop
putdone:
 ld r5,r7
 ld r7,r4
 pop r6,@sp
 add sp,#2
 ret
! r6=encoded user map, r7=offset. r0=window address, r2=status.
mapbyte:
 ld r0,r6
 srl r0,#8
 and r0,#127
 sll r0,#5
 ld r2,r7
 srl r2,#11
 or r0,r2
 out #0x00bc,r0
 in r2,#0x00be
 cp r2,#0xffff
 jr z,memfail
 out #0x00d6,r0
 ld r0,r7
 and r0,#2047
 add r0,#0xe000
 ret
rdbyte:
 call mapbyte
 cp r2,#1
 ret z
 ld r2,r0
 ldb rl1,@r2
 jr byteend
wrbyte:
 call mapbyte
 cp r2,#1
 ret z
 bit r2,#15
 jr nz,writebad
 ld r2,r0
 ldb @r2,rl1
byteend:
 ld r0,#0xffff
 out #0x00d6,r0
 clr r2
 ret
writebad:
 ld r0,#0xffff
 out #0x00d6,r0
memfail:
 ld r2,#1
 ret
.data
.global stareg,staevnt,stafcw,stapcsg
stareg: .word 0
staevnt: .word 32
stafcw: .word 34
stapcsg: .word 36
userd: .word 0
useri: .word 0
