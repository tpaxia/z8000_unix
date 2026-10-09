/* Paged Z8000 process extents, indexed by proc-table slot.
 * Bases and sizes are in 2048-byte physical frames. Swapped images contain
 * u-area, data, private text and stack, in that order; bases are then zero.
 */
struct memspace {
 unsigned base[3];
 unsigned size[3];
};
#define MEM_DATA 0
#define MEM_TEXT 1
#define MEM_STACK 2
#define MEM_FRAME 2048L
extern struct memspace memory[];
