! Sector-zero primary loader. Installer fills the /boot sector list at 0x100.
.segm
.text
.word 0x5a38
ld r8,#0x8300
ld r9,#0xff00
ld r10,@rr8
cp r10,#1
jr ult,bad
cp r10,#111
jr ugt,bad
inc r9,#2
ld r2,#0x8300
ld r3,#0
ld r12,#0x8000
ld r13,#0x100
next:
ld r0,@rr8
inc r9,#2
call @rr12
djnz r10,next
! Strip the 16-byte 0407 header; BSS is cleared by /boot's startup.
ld r2,#0x8300
ld r3,#0
ld r0,@rr2
cp r0,#0x107
jr nz,bad
ld r3,#2
ld r6,@rr2
inc r3,#2
ld r0,@rr2
add r6,r0
jr c,bad
cp r6,#0xe000
jr ugt,bad
test r6
jr z,bad
ld r3,#16
ld r4,#0x8300
ld r5,#0
ldirb @rr4,@rr2,r6
ld r15,#0xf000
ld r0,#0
push @rr14,r0
ld r0,#0x8300
push @rr14,r0
ld r0,#0x4000
push @rr14,r0
ld r0,#0
push @rr14,r0
iret
bad:
ldb rl0,#66
outb #0xf0,rl0
halt
jr bad
.org 0x100
.space 256
