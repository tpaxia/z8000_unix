! Sector-zero primary loader. Installer fills the /boot sector list at 0x100.
.unsegm
.text
.word 0x5a39
ld r9,#0xff00
ld r10,@r9
cp r10,#1
jr ult,bad
cp r10,#63
jr ugt,bad
inc r9,#2
ld r3,#0
next:
ldl rr0,@r9
inc r9,#4
call rdsect
djnz r10,next
! Strip the 24-byte s.out header and 16-byte NONSEG segment descriptor.
! BSS is cleared by /boot's startup.
ld r3,#0
ld r0,@r3
cp r0,#0xe707
jr nz,bad
ld r3,#10
ld r0,@r3
cp r0,#16
jr nz,bad
ld r3,#12
ld r0,@r3
test r0
jr nz,bad
ld r3,#14
ldl rr0,@r3
testl rr0
jr nz,bad
ld r3,#18
ld r0,@r3
cp r0,#1
jr nz,bad
ld r3,#28
ld r6,@r3
inc r3,#2
ld r0,@r3
add r6,r0
jr c,bad
cp r6,#0xe000
jr ugt,bad
test r6
jr z,bad
ld r3,#40
ld r5,#0
ldirb @r5,@r3,r6
ld r15,#0xf000
jp 0
bad:
ldb rl0,#66
outb #0xf0,rl0
halt
jr bad
rdsect:
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
jr nz,bad
bitb rl1,#7
jr nz,busy
bitb rl1,#3
jr nz,read
busy:
djnz r5,wait
jr bad
read:
ld r5,#256
readword:
in r4,#0x1f0
ld @r3,r4
inc r3,#2
djnz r5,readword
ret

.org 0x100
.space 256
