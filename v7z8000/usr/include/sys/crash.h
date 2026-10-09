#ifndef _SYS_CRASH_H
#define _SYS_CRASH_H
/* Fixed 64-byte kernel context in raw RAM, located by the /unix namelist.
 * Registers: R0-R15, FCW, encoded PC segment, PC offset (as in user cores).
 * Saved fault registers reflect the completed instruction; MMU PC identifies its
 * first word. trap_pc retains the hardware return PC before that substitution.
 */
#define KC_MAGIC 0x4b43
#define KC_VERSION 1
#define KC_PANIC 1
#define KC_FAULT 2
struct kcontext {
 unsigned magic, version, kind, upage;
 unsigned regs[19];
 unsigned tag, fault[6], trap_pc, stackseg;
};
extern struct kcontext kcrash;
#endif
