! end.s - provides _end symbol at end of BSS.
! MUST be linked last so _end is after all other BSS.

.sect .text
.sect .rom
.sect .data
.sect .bss

.define _end, __end
__end:
_end:
