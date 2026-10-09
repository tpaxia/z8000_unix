CC = cc
CFLAGS = -Wall -O2

# PCC toolchain
PCC_ROOT = ../PCC-z8000/z8000
CZ8      = $(PCC_ROOT)/cz8/cz8
AZ8      = ../tests/build/asz8k-host/asz8k
LDZ8     = ../tests/build/ldz8-host/ldz8
CPP      = cpp

# Source directories
LIBC_DIR = libc
SHELL_SRC = ../v7z8000/usr/src/cmd/sh
USR_INCLUDE = ../v7z8000/usr/include

# Shell source files
SHELL_SRCS = args.c blok.c builtin.c cmd.c ctype.c error.c expand.c fault.c \
    io.c macro.c main.c msg.c name.c print.c service.c setbrk.c \
    stak.c string.c word.c xec.c
SHELL_OBJS = $(SHELL_SRCS:.c=.b)

.PHONY: all clean

# Production bootstrap and explicit regression fixtures use s.out.
all: v7mkfs
	python3 native-cc/build.py
	cp ../tests/build/native-cc-sout/hd.img root.img

# =============================================================================
# Host tools
# =============================================================================
v7mkfs: v7mkfs.c
	$(CC) $(CFLAGS) -o $@ $<

# =============================================================================
# Generic compilation rules: C -> az8 -> b
# =============================================================================
# Preprocess with cpp, compile with cz8, assemble with az8
CPPFLAGS = -Dz8000 -Dz8002 -Isout-utils -Iasz8k/src

# =============================================================================
# libc - Z8002 user-space C library
# =============================================================================
LIBV7_DIR = libv7
SYSCALL_NAMES = $(shell python3 $(LIBC_DIR)/split-syscalls.py --names)
SYSCALL_OBJS = $(SYSCALL_NAMES:%=$(LIBV7_DIR)/%.b)
LIBC_ASM_OBJS = $(SYSCALL_OBJS) $(LIBC_DIR)/setjmp.b

$(LIBV7_DIR)/s_%.az8: $(LIBC_DIR)/syscalls.az8 $(LIBC_DIR)/split-syscalls.py Makefile
	python3 $(LIBC_DIR)/split-syscalls.py $(LIBV7_DIR)
# crt0 owns environ; syscall members share the common errno symbol.
# There is no end.b: C's "end" is now the linker's own _end, as on the PDP-11.
# fakcu.b (the dummy _cleanup that exit calls) follows exit.b for the same reason.
LIBC_C_OBJS = $(LIBC_DIR)/sbrk.b $(LIBC_DIR)/execl.b $(LIBC_DIR)/exit.b $(LIBC_DIR)/fakcu.b

$(LIBC_DIR)/crt0.b: $(LIBC_DIR)/crt0.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

$(LIBC_DIR)/%.b: $(LIBC_DIR)/%.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

$(LIBC_DIR)/%.az8: $(LIBC_DIR)/%.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

libc.a: $(LIBC_ASM_OBJS) $(LIBC_C_OBJS) libv7/csv.b Makefile
	rm -f $@
	ar cr $@ $(LIBC_C_OBJS) $(LIBC_ASM_OBJS) libv7/csv.b

# =============================================================================
# libv7.a - the Seventh Edition C library
#
# The C sources are compiled unchanged from the V7 tree. V7's PDP-11 assembly
# is replaced by the Z8000 files in libc/: doprnt.c (printf conversion),
# fpsup.c (modf, ldexp, frexp), exit.c and fakcu.c (cuexit.s, fakcu.s),
# abort.c, sbrk.c, and the system call stubs. The long and floating-point
# support routines come from the compiler's own library.
#
# The linker reads an archive once, front to back, so the order below is
# V7's own (libc/mklib): callers before the routines they call.
# Not built: mpx.c and pkon.c (multiplexor).
# Also not built: gen/stty.c, which implements stty and gtty through ioctl;
# the system call stubs provide them directly, as the kernel has both calls.
# =============================================================================
V7LIBC    = ../v7z8000/usr/src/libc
LIBV7_NAMES = getgrgid getgrnam getgrent getpass getpwnam getpwuid getpwent \
    timezone fgetc fputc getchar putchar popen freopen fgets fputs getpw \
    fseek ftell rew rdwr system fopen fdopen scanf doscan fprintf gets getw \
    printf puts putw sprintf ungetc filbuf setbuf doprnt gcvt strout flsbuf \
    endopen findiop clrerr data exit execvp getenv getlogin perror sleep \
    ttyslot ttyname abort abs atof atoi atol crypt ctime calloc malloc ecvt \
    errlst fakcu fpsup isatty l3 mktemp qsort rand swab tell ctype_ \
    index rindex strcat strncat strcmp strncmp strcpy strncpy strlen \
    sbrk execl execv mon dup2 nlist
LIBV7_OBJS = $(LIBV7_DIR)/nlist.b $(LIBV7_DIR)/object.b $(LIBV7_DIR)/soutfmt.b \
    $(filter-out $(LIBV7_DIR)/nlist.b,$(LIBV7_NAMES:%=$(LIBV7_DIR)/%.b)) \
    $(LIBC_DIR)/setjmp.b $(SYSCALL_OBJS) \
    $(LIBV7_DIR)/float.b $(LIBV7_DIR)/softfp.b $(LIBV7_DIR)/epu.b arith.b $(LIBV7_DIR)/csv.b

$(LIBV7_DIR)/%.az8: $(V7LIBC)/stdio/%.c $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(V7LIBC)/stdio -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/%.az8: $(V7LIBC)/gen/%.c $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/%.az8: $(LIBC_DIR)/%.c $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/softfp.az8: fpe/glue.c $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CZ8) < $< > $@

$(LIBV7_DIR)/epu.az8: fpe/wrappers.py Makefile
	@mkdir -p $(LIBV7_DIR)
	python3 $< $@

$(LIBV7_DIR)/csv.az8: $(PCC_ROOT)/lib/csv.az8 Makefile
	@mkdir -p $(LIBV7_DIR)
	cp $< $@

$(LIBV7_DIR)/float.az8: $(PCC_ROOT)/lib/float.az8 Makefile
	@mkdir -p $(LIBV7_DIR)
	cp $< $@

$(LIBV7_DIR)/%.b: $(LIBV7_DIR)/%.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

libv7.a: $(LIBV7_OBJS) Makefile
	rm -f $@
	ar cr $@ $(LIBV7_OBJS)

# Installed ABI headers are exported from the selected kernel's headers.
.PHONY: check-headers
check-headers:
	python3 export-headers.py --check
libc.a libv7.a: | check-headers
PUBLIC_HEADERS = $(wildcard $(USR_INCLUDE)/*.h $(USR_INCLUDE)/sys/*.h)
$(LIBV7_OBJS:.b=.az8) $(LIBC_C_OBJS:.b=.az8) $(SHELL_SRCS:.c=.az8): $(PUBLIC_HEADERS)
.INTERMEDIATE: $(LIBC_C_OBJS:.b=.az8)

# =============================================================================
# libctest - exercises libv7.a under the kernel; see libctest.c
# =============================================================================
libctest.az8: libctest.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

libctest.b: libctest.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

libctest: $(LIBC_DIR)/crt0.b libctest.b libv7.a
	$(LDZ8) -x $(LIBC_DIR)/crt0.b libctest.b libv7.a -o $@

root-libc.img: v7mkfs proto.libc init sh libctest
	./v7mkfs root-libc.img proto.libc

# CPU-bound scheduling and signal regression (no child syscalls in its loop).
preempttest.az8: preempttest.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< > preempttest.i
	$(CZ8) < preempttest.i > $@

preempttest.b: preempttest.az8 Makefile $(AZ8) asz8k.pd
	cp $< preempt.az8
	$(AZ8) -c -o $@ preempt.az8

preempttest: $(LIBC_DIR)/crt0.b preempttest.b libv7.a
	$(LDZ8) -x $(LIBC_DIR)/crt0.b preempttest.b libv7.a -o $@

signaltest.az8: signaltest.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< > signaltest.i
	$(CZ8) < signaltest.i > $@

signaltest.b: signaltest.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

sigregs.b: sigregs.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

signaltest: $(LIBC_DIR)/crt0.b signaltest.b sigregs.b libv7.a
	$(LDZ8) -x $(LIBC_DIR)/crt0.b signaltest.b sigregs.b libv7.a -o $@

root-preempt.img: v7mkfs proto.preempt init sh echo cat preempttest signaltest
	./v7mkfs root-preempt.img proto.preempt

# =============================================================================
# arith - 32-bit arithmetic library (ldiv, lrem, aldiv)
# =============================================================================
arith.az8: $(PCC_ROOT)/lib/arith.az8 Makefile
	cp $< $@

arith.b: arith.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

# =============================================================================
# init - /etc/init
# =============================================================================
init.az8: init.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

init.b: init.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

init: $(LIBC_DIR)/crt0.b init.b arith.b libc.a
	$(LDZ8) -x $(filter %.b %.a,$^) -o $@

# =============================================================================
# echo - /bin/echo
# =============================================================================
echo.az8: echo.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

echo.b: echo.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

echo: $(LIBC_DIR)/crt0.b echo.b arith.b libc.a
	$(LDZ8) -x $(filter %.b %.a,$^) -o $@

# =============================================================================
# cat - /bin/cat
# =============================================================================
cat.az8: cat.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

cat.b: cat.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

cat: $(LIBC_DIR)/crt0.b cat.b arith.b libc.a
	$(LDZ8) -x $(filter %.b %.a,$^) -o $@

# =============================================================================
# sh - /bin/sh (V7 Bourne shell)
# =============================================================================
%.az8: $(SHELL_SRC)/%.c $(wildcard $(SHELL_SRC)/*.h) $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -I$(SHELL_SRC) -I$(USR_INCLUDE) $< | $(CZ8) > $@

%.b: %.az8 Makefile $(AZ8) asz8k.pd
	$(AZ8) -c -o $@ $<

sh: $(LIBC_DIR)/crt0.b $(SHELL_OBJS) arith.b libc.a
	$(LDZ8) -x $(filter %.b %.a,$^) -o $@

# =============================================================================
# Filesystem image
# =============================================================================
root.img: v7mkfs proto.small init echo cat sh
	./v7mkfs root.img proto.small

# =============================================================================
# Clean
# =============================================================================
clean:
	rm -f v7mkfs root.img
	rm -f init init.b init.az8
	rm -f echo echo.b echo.az8
	rm -f cat cat.b cat.az8
	rm -f sh $(SHELL_OBJS) $(SHELL_SRCS:.c=.az8)
	rm -f arith.b arith.az8
	rm -f libc.a $(LIBC_DIR)/*.b
	rm -rf $(LIBV7_DIR) libv7.a libctest libctest.b libctest.az8 root-libc.img
	rm -f preempttest preempttest.b preempttest.az8 preempt.az8 preempttest.i root-preempt.img
	rm -f splittest splittest.b splittest.az8 splittest.i splitpad.b root-split.img
	rm -f idnext idnormal idnext.b idnext.az8 idnext.i libcid signalid
	rm -f ttytest ttytest.b ttytest.az8 ttytest.i root-tty.img
	rm -f signaltest signaltest.b signaltest.az8 signaltest.i sigregs.b
	rm -f $(LIBC_DIR)/errno.az8 $(LIBC_DIR)/sbrk.az8 $(LIBC_DIR)/execl.az8 $(LIBC_DIR)/crtinit.az8 $(LIBC_DIR)/exit.az8 $(LIBC_DIR)/fakcu.az8

# Terminal settings and interactive line discipline regression.
ttytest.az8: ttytest.c $(USR_INCLUDE)/sgtty.h $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< > ttytest.i
	$(CZ8) < ttytest.i > $@

ttytest: $(LIBC_DIR)/crt0.b ttytest.b libv7.a
	$(LDZ8) -x $(LIBC_DIR)/crt0.b ttytest.b libv7.a -o $@

root-tty.img: v7mkfs proto.tty init sh ttytest
	./v7mkfs root-tty.img proto.tty

# Separate instruction/data executables; ordinary programs use combined s.out.
splittest.az8: splittest.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< > splittest.i
	$(CZ8) < splittest.i > $@

idnext.az8: idnext.c $(CZ8) Makefile
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< > idnext.i
	$(CZ8) < idnext.i > $@

splittest: $(LIBC_DIR)/crt0.b splitpad.b splittest.b libv7.a $(LDZ8)
	$(LDZ8) -i -x $(LIBC_DIR)/crt0.b splitpad.b splittest.b libv7.a -o $@

idnext: $(LIBC_DIR)/crt0.b idnext.b libv7.a $(LDZ8)
	$(LDZ8) -i -x $(LIBC_DIR)/crt0.b idnext.b libv7.a -o $@

idnormal: $(LIBC_DIR)/crt0.b idnext.b libv7.a $(LDZ8)
	$(LDZ8) -x $(LIBC_DIR)/crt0.b idnext.b libv7.a -o $@

libcid: $(LIBC_DIR)/crt0.b libctest.b libv7.a $(LDZ8)
	$(LDZ8) -i -x $(LIBC_DIR)/crt0.b libctest.b libv7.a -o $@

signalid: $(LIBC_DIR)/crt0.b signaltest.b sigregs.b libv7.a $(LDZ8)
	$(LDZ8) -i -x $(LIBC_DIR)/crt0.b signaltest.b sigregs.b libv7.a -o $@

root-split.img: v7mkfs proto.split init sh splittest idnext idnormal libcid signalid
	./v7mkfs root-split.img proto.split

# Shared host copies of the installed assembler/linker.
$(AZ8): $(wildcard asz8k/src/*.[ch]) asz8k/Makefile
	$(MAKE) -C asz8k
$(LDZ8): ldz8/dispatch.c ldz8/ldso.c asz8k/src/soutfmt.c ldz8/Makefile
	$(MAKE) -C ldz8
asz8k.pd: asz8k/src/asz8k.pd
	cp $< $@
libctest preempttest signaltest init echo cat sh ttytest splittest idnext idnormal libcid signalid: $(LDZ8) Makefile

# The format adapter precedes stdio, retaining V7's remaining archive order.
$(LIBV7_DIR)/nlist.az8: $(V7LIBC)/gen/nlist.c sout-utils/object.h asz8k/src/soutfmt.h $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@
$(LIBV7_DIR)/object.az8: sout-utils/object.c sout-utils/object.h asz8k/src/soutfmt.h $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@
$(LIBV7_DIR)/soutfmt.az8: asz8k/src/soutfmt.c asz8k/src/soutfmt.h $(CZ8) Makefile
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@
