CC = cc
CFLAGS = -Wall -O2

# PCC toolchain
PCC_ROOT = ../PCC-z8000/z8000
CZ8      = $(PCC_ROOT)/cz8/cz8
AZ8      = $(PCC_ROOT)/az8/az8
LDZ8     = $(PCC_ROOT)/ldz8
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

all: v7mkfs init echo cat sh root.img

# =============================================================================
# Host tools
# =============================================================================
v7mkfs: v7mkfs.c
	$(CC) $(CFLAGS) -o $@ $<

# =============================================================================
# Generic compilation rules: C -> az8 -> b
# =============================================================================
# Preprocess with cpp, compile with cz8, assemble with az8
CPPFLAGS = -Dz8000 -Dz8002

# =============================================================================
# libc - Z8002 user-space C library
# =============================================================================
LIBC_ASM_OBJS = $(LIBC_DIR)/syscalls.b $(LIBC_DIR)/setjmp.b
# errno.b holds only uninitialized data (errno, environ). The linker loads an
# archive member only if it defines something, and such data does not count,
# so it must come after every member that refers to those names.
# There is no end.b: C's "end" is now the linker's own _end, as on the PDP-11.
# fakcu.b (the dummy _cleanup that exit calls) follows exit.b for the same reason.
LIBC_C_OBJS = $(LIBC_DIR)/sbrk.b $(LIBC_DIR)/execl.b $(LIBC_DIR)/crtinit.b $(LIBC_DIR)/exit.b $(LIBC_DIR)/fakcu.b $(LIBC_DIR)/errno.b

$(LIBC_DIR)/crt0.b: $(LIBC_DIR)/crt0.az8
	$(AZ8) -o $@ $<

$(LIBC_DIR)/%.b: $(LIBC_DIR)/%.az8
	$(AZ8) -o $@ $<

$(LIBC_DIR)/%.az8: $(LIBC_DIR)/%.c
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

libc.a: $(LIBC_ASM_OBJS) $(LIBC_C_OBJS)
	rm -f $@
	ar cr $@ $(LIBC_ASM_OBJS) $(LIBC_C_OBJS)

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
# Not built: mon.c (profiling), mpx.c and pkon.c (multiplexor), nlist.c.
# Also not built: gen/stty.c, which implements stty and gtty through ioctl;
# the system call stubs provide them directly, as the kernel has both calls.
# =============================================================================
V7LIBC    = ../v7z8000/usr/src/libc
LIBV7_DIR = libv7
LIBV7_NAMES = getgrgid getgrnam getgrent getpass getpwnam getpwuid getpwent \
    timezone fgetc fputc getchar putchar popen freopen fgets fputs getpw \
    fseek ftell rew rdwr system fopen fdopen scanf doscan fprintf gets getw \
    printf puts putw sprintf ungetc filbuf setbuf doprnt gcvt strout flsbuf \
    endopen findiop clrerr data exit execvp getenv getlogin perror sleep \
    ttyslot ttyname abort abs atof atoi atol crypt ctime calloc malloc ecvt \
    errlst fakcu fpsup isatty l3 mktemp qsort rand swab tell ctype_ \
    index rindex strcat strncat strcmp strncmp strcpy strncpy strlen \
    sbrk execl crtinit
LIBV7_OBJS = $(LIBV7_NAMES:%=$(LIBV7_DIR)/%.b) \
    $(LIBC_DIR)/setjmp.b $(LIBC_DIR)/syscalls.b \
    $(LIBV7_DIR)/float.b $(LIBV7_DIR)/softfp.b arith.b $(LIBV7_DIR)/errno.b

$(LIBV7_DIR)/%.az8: $(V7LIBC)/stdio/%.c
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(V7LIBC)/stdio -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/%.az8: $(V7LIBC)/gen/%.c
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/%.az8: $(LIBC_DIR)/%.c
	@mkdir -p $(LIBV7_DIR)
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

$(LIBV7_DIR)/softfp.az8: $(PCC_ROOT)/lib/softfp.c
	@mkdir -p $(LIBV7_DIR)
	$(CZ8) < $< > $@

$(LIBV7_DIR)/float.az8: $(PCC_ROOT)/lib/float.az8
	@mkdir -p $(LIBV7_DIR)
	cp $< $@

$(LIBV7_DIR)/%.b: $(LIBV7_DIR)/%.az8
	$(AZ8) -o $@ $<

libv7.a: $(LIBV7_OBJS)
	rm -f $@
	ar cr $@ $(LIBV7_OBJS)

# =============================================================================
# libctest - exercises libv7.a under the kernel; see libctest.c
# =============================================================================
libctest.az8: libctest.c
	$(CPP) $(CPPFLAGS) -nostdinc -I$(USR_INCLUDE) $< | $(CZ8) > $@

libctest.b: libctest.az8
	$(AZ8) -o $@ $<

libctest: $(LIBC_DIR)/crt0.b libctest.b libv7.a
	$(LDZ8) -x -R 0 $(LIBC_DIR)/crt0.b libctest.b libv7.a -o $@

root-libc.img: v7mkfs proto.libc init sh libctest
	./v7mkfs root-libc.img proto.libc

# =============================================================================
# arith - 32-bit arithmetic library (ldiv, lrem, aldiv)
# =============================================================================
arith.az8: $(PCC_ROOT)/lib/arith.az8
	cp $< $@

arith.b: arith.az8
	$(AZ8) -o $@ $<

# =============================================================================
# init - /etc/init
# =============================================================================
init.az8: init.c
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

init.b: init.az8
	$(AZ8) -o $@ $<

init: $(LIBC_DIR)/crt0.b init.b arith.b libc.a
	$(LDZ8) -x -R 0 $^ -o $@

# =============================================================================
# echo - /bin/echo
# =============================================================================
echo.az8: echo.c
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

echo.b: echo.az8
	$(AZ8) -o $@ $<

echo: $(LIBC_DIR)/crt0.b echo.b arith.b libc.a
	$(LDZ8) -x -R 0 $^ -o $@

# =============================================================================
# cat - /bin/cat
# =============================================================================
cat.az8: cat.c
	$(CPP) $(CPPFLAGS) -I$(USR_INCLUDE) $< | $(CZ8) > $@

cat.b: cat.az8
	$(AZ8) -o $@ $<

cat: $(LIBC_DIR)/crt0.b cat.b arith.b libc.a
	$(LDZ8) -x -R 0 $^ -o $@

# =============================================================================
# sh - /bin/sh (V7 Bourne shell)
# =============================================================================
%.az8: $(SHELL_SRC)/%.c
	$(CPP) $(CPPFLAGS) -I$(SHELL_SRC) -I$(USR_INCLUDE) $< | $(CZ8) > $@

%.b: %.az8
	$(AZ8) -o $@ $<

sh: $(LIBC_DIR)/crt0.b $(SHELL_OBJS) arith.b libc.a
	$(LDZ8) -x -R 0 $^ -o $@

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
	rm -f $(LIBC_DIR)/errno.az8 $(LIBC_DIR)/sbrk.az8 $(LIBC_DIR)/execl.az8 $(LIBC_DIR)/crtinit.az8 $(LIBC_DIR)/exit.az8 $(LIBC_DIR)/fakcu.az8
