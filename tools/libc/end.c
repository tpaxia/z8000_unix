/*
 * end.c - provides _end symbol at end of BSS+commons.
 * MUST be linked last.
 *
 * ACK's linker places "common" symbols (tentative definitions) in a
 * separate area after .bss.  Using a tentative definition here (instead
 * of a .bss label in assembly) ensures _end lands in that common area.
 * Because end.o is linked last, _end should be the last common symbol.
 */
char end[1];
