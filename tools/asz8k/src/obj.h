/*
 * @(#)obj.h	3.3
 *
 * Internal encoder relocation actions and section attributes.
 *
 * Copyright 1981 by John D. Polstra and Robert M. McClure.
 * All rights reserved.
 */

/*
 * Object file relocation actions.
 */
#define	RANOP	0		/* no relocation operation */
#define	RAA16	0x8000		/* add base to word field */
#define	RAA8	0x4000		/* add base to byte field */
#define	RAA32	0xc000		/* add base to long field */
#define	RAA16M	0x2000		/* add base to word field (MSB first) */
#define	RAA32M	0xa000		/* add base to long field (MSB first) */
#define	RAZSS	0x6000		/* relocate Z8001 short seg address */
#define	RAZLS	0xe000		/* relocate Z8001 long seg address */
#define	RASEG	0x1000		/* 8086 segment relocation */
#define RAOFF	0x9000		/* 8086 offset relocation */
#define RAJ11	0x5000		/* 8051 11-bit jump target relocation */
#define RASOFF	0xd000		/* 8086 short offset relocation */
#define	RAZOF	0x3000		/* relocate Z8001 16-bit offset */
#define	RAMSK	0xf000		/* mask for relocation action field */
/*
 * Object file relocation bases.
 */
#define	RBABS	0		/* absolute */
#define	RBSEC	1		/* section */
#define	RBUND	255		/* undefined */
#define	RBEXT	256		/* external */
#define	RBMSK	0x0fff		/* mask for relocation base field */
/*
 * Section attributes.
 */
#define	SENOX	0001		/* not executable */
#define	SENOW	0002		/* not writeable */
#define	SENOR	0004		/* not readable */
#define	SECOM	0010		/* common */
#define	SEFIX	0020		/* base address fixed */
