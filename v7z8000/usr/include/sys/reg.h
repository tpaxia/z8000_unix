/* Z8000 common trap frame, indexed through u_ar0. R13/R14 and USP
 * are saved separately; the complete public core image is u_regs below.
 */
#define R0 0
#define R1 1
#define R2 2
#define R3 3
#define R4 4
#define R5 5
#define R6 6
#define R7 7
#define R8 8
#define R9 9
#define R10 10
#define R11 11
#define R12 12
#define RPS 14
#define PCSEG 15
#define PC 16

/* Complete u_regs image: R0-R15 at indices 0-15, then FCW and PC. */
#define UREG_SP 15
#define UREG_FCW 16
#define UREG_SEG 17
#define UREG_PC 18
#define UREG_NREG 19
