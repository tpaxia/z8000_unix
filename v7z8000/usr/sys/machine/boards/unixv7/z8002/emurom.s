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
 jp 0x01e0
 .org 0x01e0
 ld r0,#1
 out #0x00d8,r0
 jp 0x01f0
