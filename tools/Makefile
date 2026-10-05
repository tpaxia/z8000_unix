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
LIBC_C_OBJS = $(LIBC_DIR)/sbrk.b $(LIBC_DIR)/execl.b $(LIBC_DIR)/crtinit.b $(LIBC_DIR)/exit.b $(LIBC_DIR)/errno.b

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
	rm -f $(LIBC_DIR)/errno.az8 $(LIBC_DIR)/sbrk.az8 $(LIBC_DIR)/execl.az8 $(LIBC_DIR)/crtinit.az8 $(LIBC_DIR)/exit.az8
