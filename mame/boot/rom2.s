! Direct-load bring-up ROM. Switch at a mirrored instruction address.
.unsegm
.text
.global _start
_start:
 .word 0,0x4000,0x0010
 .org 0x0010
 ld r15,#0xfff0
 ld r0,#0xd000
 ldctl psapoff,r0
 ld r0,#0xfff0
 ldctl nspoff,r0
 ld r1,#4032
 ld r2,#0x4040
 ld r3,#32
map_kernel:
 out #0x00bc,r1
 out #0x00be,r2
 inc r1,#1
 inc r2,#1
 djnz r3,map_kernel
 ! Service instruction context: kernel gates and banked arithmetic.
 ld r1,#4064
 ld r2,#0x4040
 ld r3,#32
map_service:
 out #0x00bc,r1
 out #0x00be,r2
 inc r1,#1
 inc r2,#1
 djnz r3,map_service
 ld r1,#4080
 ld r2,#0x41f0
 ld r3,#8
map_engine:
 out #0x00bc,r1
 out #0x00be,r2
 ld r0,r1
 sub r0,#4032
 out #0x00bc,r0
 out #0x00be,r2
 inc r1,#1
 inc r2,#1
 djnz r3,map_engine
 ld r0,#0x017e
 out #0x00b8,r0
 ! Identity RAM contexts for the standalone loader and physical kernel copy.
 ld r1,#64
 ld r2,#0x4040
 ld r3,#64
map_boot:
 out #0x00bc,r1
 out #0x00be,r2
 inc r1,#1
 inc r2,#1
 djnz r3,map_boot
 ! Copy the instruction-map handoff into the standalone instruction page.
 clr r0
 out #0x00d6,r0
 ld r4,0xe1e8
 ld r5,0xe1ea
 ld r0,#96
 out #0x00d6,r0
 ld 0xe1e8,r4
 ld 0xe1ea,r5
 ld r0,#127
 out #0x00d6,r0
 clr r0
 clr r1
 ld r3,#0xe600
 call rdsect
 ld r0,#0xffff
 out #0x00d6,r0
 ld r0,#3
 out #0x00da,r0
 jp 0x01e0
 .org 0x01e0
 ld r0,#3
 out #0x00d8,r0
 jp 0xfe02
 .org 0x0200
 ! Copied to 0xfdf0 in both bootstrap and kernel instruction maps.
 ld r0,#1
 out #0x00da,r0
 out #0x00d8,r0
 ld r15,#0xfff0
 ld r0,#0xd000
 ldctl psapoff,r0
 ld r0,#0xfff0
 ldctl nspoff,r0
 jp 0x01f0
 .org 0x0240
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
jr nz,ioerror
bitb rl1,#7
jr nz,busy
bitb rl1,#3
jr nz,read
busy:
djnz r5,wait
jr ioerror
read:
ld r5,#256
readword:
in r4,#0x1f0
ld @r3,r4
inc r3,#2
djnz r5,readword
ret

ioerror:
 ldb rl0,#69
 outb #0xf0,rl0
 halt
 jr ioerror
