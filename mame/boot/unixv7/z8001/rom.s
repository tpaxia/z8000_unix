! Board firmware: read sector zero into 3:fe00, then execute it.
! No Unix executable or filesystem knowledge lives in ROM.
.segm
.text
.word 0,0xc000,0x8000,0x0010
.org 0x10
ld r1,#64
ld r2,#0x4040
ld r3,#64
map:
out #0xbc,r1
out #0xbe,r2
inc r1,#1
inc r2,#1
djnz r3,map
ld r14,#0x8300
ld r15,#0xfd00
ld r0,#0
ld r1,#0
ld r2,#0x8300
ld r3,#0xfe00
ld r12,#0x8000
ld r13,#0x100
call @rr12
ld r2,#0x8300
ld r3,#0xfe00
ld r0,@rr2
cp r0,#0x5a39
jr nz,error
inc r3,#2
jp @rr2
error:
ldb rl0,#69
outb #0xf0,rl0
halt
jr error

! Fixed firmware service at 0:0100: RR0=LBA (28 bit), RR2=destination.
! Clobbers R0,R1,R4,R5; advances destination by 512. Masked interrupts.
.org 0x100
outb #0x1f3,rl1
outb #0x1f4,rh1
outb #0x1f5,rl0
ldb rl4,rh0
and r4,#15
or r4,#0xe0
outb #0x1f6,rl4
ldb rl1,#1
outb #0x1f2,rl1
ldb rl1,#0x20
outb #0x1f7,rl1
ld r5,#0xffff
wait:
inb rl1,#0x1f7
bitb rl1,#0
jr nz,error
bitb rl1,#7
jr nz,busy
bitb rl1,#3
jr nz,read
busy:
djnz r5,wait
jr error
read:
ld r5,#256
readword:
in r4,#0x1f0
ld @rr2,r4
inc r3,#2
djnz r5,readword
ret

! Fixed kernel handoff service is supplied by build_rom.py at 0:0200.
