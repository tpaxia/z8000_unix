/* ZEUS s.out disk layout. Never write a C structure to the object file.
 * Source: Zilog s.out.h 1.5 and ZEUS a.out(5) 1.29.
 * These routines also build with V7 PCC (16-bit int, 32-bit long).
 */
#ifndef SOUTFMT_H
#define SOUTFMT_H
#define SO_HEAD 24
#define SO_SEG 16
#define SO_SYM 14
#define SO_SMAG 0xe607
#define SO_NMAG 0xe707
#define SO_SID 0xe611
#define SO_NID 0xe711
#define SO_STRIP 1
#define SO_CODE 1
#define SO_DATA 2
#define SO_BSS 4
#define SO_BOUND 128
#define SO_UNDEF 0
#define SO_ABS 1
#define SO_TEXTSYM 2
#define SO_DATASYM 3
#define SO_BSSSYM 4
#define SO_EXTERNAL 32
#define SO_SEGMENTED 64
#define SO_REXT 8
#define SO_ROFF 0
#define SO_RSEG 1
#define SO_RSHORT 2
#define SO_R12 3
#define SO_R16 4
extern unsigned so_get16();
extern long so_get32();
extern int so_put16(), so_put32();
extern int so_header(), so_segment(), so_symbol();
extern int so_reloc();
#endif
