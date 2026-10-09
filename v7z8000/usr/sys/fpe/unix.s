! Unix service in segment 127. Pages 30/31 alias the current kernel u-area.
! Arithmetic and decoding come from the preserved Zilog fpe.z8k.
.text
.segm
.global fpentry,epu
fpentry:
 sub r15,#32
 ldm @rr14,r0,#14
 ld r0,#0x4000
 ldctl fcw,r0
.unsegm
 ld 28(r15),r14
 ldctl r0,nspoff
 ld 30(r15),r0
 ld r9,r15
 ld r0,#0xc000
 ldctl fcw,r0
.segm
 .word 0x5f00,0x8100,0x0208
 ! C returns masked, in segmented mode, with the same full frame.
 ld r0,#0x4000
 ldctl fcw,r0
.unsegm
 ld r14,28(r15)
 ld r0,30(r15)
 ldctl nspoff,r0
 ld r0,#0xc000
 ldctl fcw,r0
.segm
 ldm r0,@rr14,#14
 add r15,#32
 iret

! Called from kernel fprun with r9=full frame, r13=208-byte workspace,
! r10=user D segment, r11=user I backing segment. SEG call/return.
.org 0x80
.global fpe_run
fpe_run:
 ld r0,#0x5800
 ldctl fcw,r0
.unsegm
 ld userd,r10
 ld useri,r11
 clr 136(r13)
 call epu
 ld r0,r2
 ld r1,#0xc000
 ldctl fcw,r1
.segm
 ret

.unsegm
.global gettext,getmem,putmem
gettext:
 ld r6,useri
 ld r0,#0xc000
 ldctl fcw,r0
.segm
 ld r3,@rr6
 ld r0,#0x5800
 ldctl fcw,r0
.unsegm
 clr r2
 ret

! r3=byte count; get: rr6=user source, rr4=service destination.
! put: rr4=user destination, rr6=service source. Preserve r8-r13.
getmem:
 cp r3,#20
 jr ugt,memfail
 ld r0,r7
 add r0,r3
 jr nc,getvalid
 test r0
 jr nz,memfail
getvalid:
 ld r6,userd
getloop:
 ld r0,#0xc000
 ldctl fcw,r0
.segm
 ldb rl1,@rr6
 ld r0,#0x5800
 ldctl fcw,r0
.unsegm
 ldb @r5,rl1
 inc r7,#1
 inc r5,#1
 djnz r3,getloop
 clr r2
 ret
putmem:
 cp r3,#20
 jr ugt,memfail
 ld r0,r5
 add r0,r3
 jr nc,putvalid
 test r0
 jr nz,memfail
putvalid:
 ld r4,userd
putloop:
 ldb rl1,@r7
 ld r0,#0xc000
 ldctl fcw,r0
.segm
 ldb @rr4,rl1
 ld r0,#0x5800
 ldctl fcw,r0
.unsegm
 inc r7,#1
 inc r5,#1
 djnz r3,putloop
 clr r2
 ret
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
