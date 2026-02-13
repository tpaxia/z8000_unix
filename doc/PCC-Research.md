# The Portable C Compiler: History, Sources, and Modern Revival

## 1. Origins: PCC in Seventh Edition Unix (1976-1979)

### Author and Motivation

The Portable C Compiler was written by **Stephen C. Johnson** at Bell
Laboratories in the mid-1970s.  Before PCC, the C compiler in use was Dennis
Ritchie's original compiler (the "DMR compiler"), which was written
specifically for the PDP-11 using recursive descent parsing and was tightly
coupled to PDP-11 machine assumptions.  With the arrival of new hardware --
the Interdata 8/32 minicomputer and the impending DEC VAX-11/780 -- Johnson
redesigned the compiler from scratch to prioritize portability.

Johnson's work built in part on ideas from Alan Snyder's 1975 MIT Master's
thesis, "A Portable Compiler for the Language C" (MIT-LCS-TR-149).

PCC debuted in Seventh Edition Unix (V7) in 1979 and was eventually ported to
more than 200 architectures.

### Architecture: The Two-Pass Design

PCC used a clean two-pass design, preceded by a C preprocessor:

**Pass 1 (Frontend):** Approximately 4,600 lines of C code.  Performed lexical
analysis, syntax parsing (using a Yacc-generated parser), symbol table
management, type checking, and construction of expression trees.  Of these,
only about 600 lines (~12%) were machine-dependent -- primarily register names,
subroutine prologs/epilogs, switch statement code generation, and storage
allocation.

**Pass 2 (Backend):** Approximately 3,400 lines of C code.  Read the
intermediate expression trees (written in Polish Prefix notation to an
intermediate file) and performed code generation via a template-matching
mechanism.  About 1,000 lines (~30%) were machine-dependent.  The key
innovation was a set of code templates in `table.c`: "If we have a subtree of
a given shape, and we have a goal to achieve, and we have sufficient free
resources, then we may emit an instruction or instructions and rewrite the
subtree."

**Total:** Roughly 8,000 lines of source, of which about 1,600 lines (20%)
were machine-dependent.

### Key Technical Features

- Yacc-generated parser (replacing Ritchie's hand-written recursive descent)
- Expression trees as the central intermediate representation
- Polish Prefix notation for the intermediate file format between passes
- Sethi-Ullman numbering for estimating scratch register needs per subtree
- Template-based code generation with `match`, `expand`, and `reclaim` routines
- Machine-dependent code isolated into specific files: `macdefs`, `mac2defs`,
  `table.c`, `local.c`, `order.c`, `local2.c`

### Language Support (Extended K&R C)

The V7 PCC implemented an extended version of K&R C that included:
- The `void` return type
- Enumerations (`enum`)
- Structure assignment and passing/returning structs by value
- Robust syntax error recovery
- More thorough validity checking than contemporary compilers

### Original Target Machines

- PDP-11 (the original Unix platform)
- Interdata 8/32 (the first non-PDP Unix port)
- DEC VAX-11/780
- IBM System/370 (under both OS and TSS)
- Honeywell 6000
- SEL 86
- Data General Nova and Eclipse
- A Bell System processor

Johnson's famous observation: "If you need a C compiler written for a machine
with a reasonable architecture, the compiler is already three-quarters
finished!"

### Retargeting Procedure

To port PCC to a new architecture:

1. Define register names and counts in `mac2defs`
2. Define data sizes and alignments in `macdefs`
3. Write the instruction template table in `table.c`
4. Implement machine-specific routines in `order.c`, `local.c`, `local2.c`
5. Write the Sethi-Ullman computation routine (`sucomp`)


## 2. Dominance and Decline (1980s-1990s)

By the early 1980s, the majority of C compilers in existence were based on
PCC.  It became the default compiler in BSD Unix (from 4BSD through 4.4BSD in
1994), AT&T System III and System V, and numerous commercial systems.  PCC
enabled the explosion of Unix across the Motorola 68000, Intel 8086, Zilog
Z8000, and other microprocessor architectures.

The GNU C Compiler (GCC), first released in 1987 by Richard Stallman, gradually
displaced PCC with broader language support, an open-source license, and
extensive support for new RISC architectures.  When BSD 4.4 shipped in 1994,
GCC replaced PCC as the default BSD compiler.


## 3. The MIT Back-End Ports (eunuchs/unix-archive)

### Location

The Unix Heritage Society (TUHS) archive, mirrored at:
https://github.com/eunuchs/unix-archive/tree/master/Applications/Portable_CC

### Contents

The `Applications/Portable_CC/` directory contains four zip archives and a
README, donated to TUHS by archivist Al Kossow in August 2002:

| File        | Target                                              |
|-------------|-----------------------------------------------------|
| `8086.zip`  | Intel 8086 (includes libc, 8087 floating-point)     |
| `286.zip`   | Intel 80286                                         |
| `68000.zip` | Motorola 68000                                      |
| `16032.zip` | National Semiconductor NS 16032 (later NS 32016)    |

The README reads:

> Al Kossow sent in these versions of the portable C compiler for the 8086,
> Z8000, and 68000 done by MIT's Laboratory for Computer Science.  Enjoy.

(The README mentions Z8000 but the actual fourth archive is for the NS 16032.)

### Era

These sources date from approximately 1981-1985, based on:

- They target 4.1BSD on VAX-11/780 as the host platform (4.1BSD was released
  in 1981)
- The NS 16032 first shipped in 1982; the Intel 80286 was announced in
  February 1982
- MIT LCS was actively producing PCC back-end ports as student and research
  projects during this period

### Relationship to V7 PCC

These are **PCC back-end ports** -- cross-compiler code generators built on top
of Johnson's original PCC framework.  They ran on a VAX host and produced
assembly code for the target microprocessors.  They represent the "retargeting"
workflow that Johnson designed PCC for: take the machine-independent frontend,
write new machine description files, and produce a C compiler for a new
architecture.

The historical lineage: Alan Snyder at MIT wrote a precursor portable compiler
(1975) -> Johnson at Bell Labs built PCC (1976-1979) -> MIT LCS researchers
retargeted PCC to early 1980s microprocessors -> those back-ends are what is
preserved in this archive.

These are the closest available sources to the **original V7-era PCC**.


## 4. The PCC Revival Project (2007-Present)

### Origin and Motivation

In 2007, Swedish developer Anders Magnusson ("Ragge"), along with Peter A.
Jonsson, restarted development of PCC.  The project was hosted at
http://pcc.ludd.ltu.se/ and released under a BSD license, funded by the BSD
Fund.

Motivations within the BSD community:

- **Licensing:** GCC's GPL license was at odds with BSD's permissive licensing
  philosophy
- **Compiler monopoly:** Theo de Raadt (OpenBSD) cited "fighting against an
  open source monopoly"
- **GCC bloat and speed:** GCC had grown to 15+ million lines; PCC was
  measured as 5-10x faster at compilation
- **Architecture support:** GCC regularly broke support for architectures the
  BSDs still used
- **Auditability:** GCC was "huge and quite daunting" and difficult to audit
  for security

In September 2007, PCC was imported into both the NetBSD pkgsrc and OpenBSD
source trees.  By 2010, OpenBSD's kernel was successfully compiled with PCC.

### Relationship to V7 PCC

The revived PCC is a direct descendant of Johnson's V7 code but has been
extensively rewritten:

- About 50% of the frontend code was rewritten
- About 80% of the backend code was rewritten
- A completely new C99-compliant preprocessor was written
- The register allocator, optimizer, and code generator are substantially new

What was preserved: the two-pass design philosophy, the Yacc-based parser
approach, the modular portability strategy, and the expression-tree
intermediate representation.

### V7 PCC vs. Modern PCC Revived

| Aspect           | V7 PCC (~1979)                   | Modern PCC Revived          |
|------------------|----------------------------------|-----------------------------|
| Language         | Extended K&R C                   | Full C99 + later additions  |
| Code size        | ~8,000 lines (one target)        | Larger, still tiny vs GCC   |
| Frontend         | ~4,600 lines, Yacc parser        | ~50% rewritten, Yacc-based  |
| Backend          | ~3,400 lines, template matching  | ~80% rewritten              |
| Register alloc.  | Sethi-Ullman numbering           | Enhanced multi-register     |
| Preprocessor     | V6/V7-era cpp                    | Completely new, C99         |
| IR format        | Polish Prefix on intermediate file | Tree-based, improved      |
| Optimization     | Minimal                          | Substantially improved      |
| Compile speed    | Fast                             | 5-10x faster than GCC       |
| Targets          | PDP-11, VAX, Interdata, IBM 370  | i386, amd64, ARM, aarch64, RISC-V, MIPS, PowerPC, SPARC64, VAX, PDP-10/11 (~18 archs) |
| OS support       | V7 Unix                          | Linux, *BSD, Darwin, Windows, Android |

### Release Timeline

| Date      | Milestone                                              |
|-----------|--------------------------------------------------------|
| 2007      | Revival begins; imported into OpenBSD and NetBSD       |
| 2010      | OpenBSD kernel successfully compiled with PCC          |
| 2011      | PCC 1.0 released                                       |
| 2014      | PCC 1.1.0 released (last formal release)               |
| 2022-2023 | Development build 1.2.0.DEVEL, patches still applied   |
| 2025      | Canonical upstream moves to GitHub (PortableCC/pcc)    |


## 5. GitHub Mirrors of PCC Revived

All three of these repositories mirror the same PCC Revived project from its
original CVS repository at pcc.ludd.ltu.se, frozen at different points in time:

### arnoldrobbins/pcc-revived

- URL: https://github.com/arnoldrobbins/pcc-revived
- Last code update: October 30, 2023 (DATESTAMP 20231021)
- 49 stars, 9 forks
- The most complete mirror: includes `pcc-libs`, the `cc2` experimental
  backend, and support for aarch64 and RISC-V (added upstream after 2018)
- Stopped syncing when the upstream CVS server went offline
- 18 target architectures

### IanHarvey/pcc

- URL: https://github.com/IanHarvey/pcc
- Last code update: September 16, 2018 (DATESTAMP 20180916)
- 124 stars (most popular), 33 forks
- Imported from a CVS snapshot using cvs2git
- Does not include pcc-libs; compiler only
- 16 target architectures (no aarch64 or RISC-V)
- The README disclaims maintenance: "THIS IS NOT MY CODE!"

### sylvandb/pcc-portable-C-compiler

- URL: https://github.com/sylvandb/pcc-portable-C-compiler
- Last code update: May 14, 2020 (DATESTAMP 20200514, on `__crap-clone__` branch)
- 3 stars, 0 forks
- Imported using the Crap CVS-to-git converter
- Uniquely preserves CVS branch structure: `r-1-0-0`, `r-1-0-1`, `r-1-1-0`,
  `BSD_44`, `janeno_1`
- Does not include pcc-libs
- 16 target architectures (no aarch64 or RISC-V)
- The `master` branch contains only README and LICENSE; code is on
  `__crap-clone__`

### Summary

| Attribute             | pcc-revived      | IanHarvey/pcc | sylvandb          |
|-----------------------|------------------|---------------|-------------------|
| Last code update      | Oct 2023         | Sep 2018      | May 2020          |
| Includes pcc-libs     | Yes              | No            | No                |
| Has aarch64/RISC-V    | Yes              | No            | No                |
| Preserves CVS branches | No             | No            | Yes               |
| Currently maintained  | No (CVS offline) | No            | No                |

### The New Canonical Upstream

As of mid-2025, the canonical PCC project has moved to a new official GitHub
organization: **https://github.com/PortableCC/pcc**

- Created May 31, 2025
- Actively maintained with commits as recent as February 2026
- 65 stars, 10 forks
- Contains active development (bug fixes, new builtins, etc.)

All three mirrors listed above are now obsolete.


## 6. Summary of All Sources

| Source | Era | What It Contains |
|--------|-----|-----------------|
| eunuchs/unix-archive Portable_CC | 1981-1985 | MIT LCS back-end ports of V7-era PCC for 8086, 80286, 68000, NS 16032 (cross-compilers hosted on VAX/4.1BSD) |
| arnoldrobbins/pcc-revived | 2007-2023 | Most complete CVS mirror of PCC Revived (modern C99 rewrite); includes pcc-libs, aarch64, RISC-V |
| IanHarvey/pcc | 2007-2018 | CVS snapshot of PCC Revived, frozen Sep 2018; compiler only |
| sylvandb/pcc-portable-C-compiler | 2007-2020 | CVS mirror of PCC Revived, frozen May 2020; preserves CVS branch history |
| PortableCC/pcc | 2007-present | **Current canonical upstream**; actively maintained on GitHub |


## 7. Compatibility: Can Modern PCC Revived Compile V7 Code?

### Language Level: Mostly Yes

Modern PCC Revived still silently accepts the key K&R idioms V7 code uses:

| V7 Idiom                                     | PCC Revived Behavior                        |
|-----------------------------------------------|---------------------------------------------|
| Implicit `int` return types                   | Silently accepted (`NORETYP` flag, no error)|
| K&R function definitions                      | Fully supported via `oldstyle` flag          |
| Implicit function declarations                | Auto-declared as `extern int f()`            |
| `register c;` (implicit int on variables)     | Accepted                                     |
| `+=` operators (V7 already uses modern form)  | No issue                                     |

### Environment Level: No

The real barriers are not the language but the toolchain ecosystem:

1. **Missing V7 sysroot:** PCC Revived does not ship V7 headers or libraries.
   V7's `<sys/types.h>`, `<sys/stat.h>`, etc. define PDP-11-specific types
   (`ino_t` as 16-bit, `struct direct` with 14-char filenames).

2. **Assembly stubs:** V7 system calls are implemented as 56 pure PDP-11
   assembly files (`.s` files using `sys` trap instructions).  These need the
   V7 assembler, not PCC.

3. **PDP-11 backend quality:** PCC Revived has a PDP-11 target
   (`arch/pdp11/`) with correct 16-bit int/pointer definitions, but testing
   by retrocomputing enthusiasts found bugs: assembler incompatibilities
   (BSD-style octal vs decimal), unsupported synthetic branch instructions
   (`JBR`/`JCC`), stack frame bugs in long division, and a missing runtime
   library.

4. **Linker and a.out format:** V7's object format differs from modern tools.

### The Fundamental Problem

**Modern PCC Revived is itself a C99 program.  It cannot be compiled on V7.**
It is a modern compiler that happens to descend from V7 PCC, but the code has
been 50-80% rewritten and requires a modern C99 build environment.  It is not
suitable as a cross-compiler hosted on V7.

### Alternatives for Compiling Old Unix Code

- **ACK** (Amsterdam Compiler Kit) -- works best out of the box for PDP-11
- **knrcc** (https://github.com/AoiMoe/knrcc) -- preserved copy of the actual
  V7 K&R C compiler
- Robert Nordier's **V7/x86** port used ACK, not PCC


## 8. Porting V7 to the Zilog Z8001: Strategy

### The Goal

Port Unix V7 to the Z8001 (the segmented variant of the Zilog Z8000 family).

### The Right Compiler: V7's Own PCC

None of the modern PCC Revived GitHub repositories are suitable for this task.
They are C99 programs that cannot run on V7, and they target modern operating
systems.

The correct starting point is **the PCC that ships with V7 itself**, located in
the V7 source tree at:

- `/usr/src/cmd/mip/` -- machine-independent part (frontend)
- `/usr/src/cmd/cc/` -- PDP-11 machine-dependent part (backend)

This is the compiler that was specifically designed for retargeting.  Johnson
and Ritchie used exactly this approach for the Interdata 8/32 port (1977) and
the VAX 32V port (1978).

The V7 source tree is available from TUHS:
https://www.tuhs.org/cgi-bin/utree.pl?file=V7/usr/src/cmd

### The MIT Back-Ends as Templates

The MIT LCS back-end ports in the unix-archive
(https://github.com/eunuchs/unix-archive/tree/master/Applications/Portable_CC)
are invaluable as **working examples** of how to retarget V7-era PCC.  They
show the complete set of machine-dependent files needed for the 8086, 68000,
80286, and NS 16032.  The 8086 back-end is particularly relevant since the
8086 is also a 16-bit segmented architecture.

The README mentions Z8000 as one of the targets, but **no Z8000 PCC back-end
source code survives in the archive**.  The fourth archive (`16032.zip`)
contains the NS 16032 back-end instead.

### Historical Precedent: Z8000 Unix Ports

Multiple groups successfully ported Unix to the Z8000 family in 1980-1982:

| System                    | Processor | Unix Version | Year |
|---------------------------|-----------|-------------|------|
| Onyx C8002 (ONIX)        | Z8002     | V7          | 1980 |
| Zilog System 8000 (ZEUS) | Z8001     | V7 + BSD    | 1981 |
| Commodore 900 (Coherent) | Z8001     | V7 work-alike | 1983 |
| Central Data (Xenix)     | Z8001     | Xenix       | 1981 |
| EAW P8000 (WEGA)         | U8001*    | System III  | 1987 |

(*U8001 = East German Z8001 clone)

Onyx's Z8002 port required only **60 lines of V7 C code changes** because they
used the non-segmented Z8002 with a custom MMU, making it PDP-11-like.

Zilog's ZEUS used the segmented Z8001 with three Z8010 MMUs -- a harder path.

### Z8001 Architecture Summary

- 16-bit word, 16 general-purpose registers (R0-R15)
- Registers can be accessed as bytes (RH0-RL7), words (R0-R15), long pairs
  (RR0-RR14), or quads (RQ0-RQ12)
- Segmented addressing: 7-bit segment + 16-bit offset = 23-bit physical (8 MB)
- Pointers are 32 bits (segment:offset packed into a 32-bit word)
- System/Normal execution modes with separate stack pointers
- R15 = stack pointer, R14 = stack segment number (RR14 = segmented SP)
- Works with Z8010 MMU for memory protection and translation

### The Segmentation Problem

The Z8001's segmented memory model creates a fundamental challenge:

- `sizeof(int) = 2` but `sizeof(char *) = 4` (32-bit segmented pointer)
- V7 code frequently conflates `int` and `char *` -- functions returning
  pointers declared as returning `int`, casting between int and pointers
- Pointer arithmetic cannot cross segment boundaries (64 KB max per segment)
- `malloc()` cannot return blocks spanning segments

### Recommended Approach: Small Model First

Follow Onyx's strategy -- treat the Z8001 like a "Z8002 with MMU hardware":

1. Restrict each process to a single segment (64 KB), using 16-bit pointers
2. Use separate segments for text and data (split I/D, like PDP-11), giving
   128 KB effective per process
3. The kernel manages segment allocation for process isolation via Z8010 MMUs
4. This makes userland code trivially portable from PDP-11

Once the system boots and runs with the small model, segmented "large model"
support (32-bit pointers, multi-segment processes) can be added later.

### Cross-Compilation Strategy

The historically authentic approach (exactly how the Interdata and VAX ports
were done):

1. **Run V7 on a PDP-11 emulator** (SIMH)
2. **Retarget V7's PCC** to generate Z8001 assembly:
   - Write `macdefs.h` -- Z8001 data sizes, alignments, register counts
   - Write `mac2defs.h` -- Z8001 register names, stack frame parameters
   - Write `table.c` -- Z8001 instruction templates (the bulk of the work)
   - Write `local.c` -- prologs/epilogs, switch code, NAME node rewriting
   - Write `local2.c` -- register names, opcodes, address output
   - Write `order.c` -- Sethi-Ullman computation, instruction ordering
   - Use the 8086 MIT back-end as a starting template (closest: 16-bit,
     segmented)
3. **Write a Z8001 cross-assembler** (or adapt an existing one)
4. **Write a cross-linker** for the Z8001 a.out/COFF format
5. **Cross-compile V7** entirely on the PDP-11 emulator
6. **Transfer binaries** to the Z8001 target (or emulator)

### Available Z8000 Toolchains

| Tool                    | Status           | Notes                                    |
|-------------------------|------------------|------------------------------------------|
| GNU binutils (z8k)      | Still in binutils | Assembler + linker for Z8001 and Z8002; `.z8001`/`.z8002` directives; COFF format |
| GCC 2.9 z8k (Cygnus)   | Archived         | `-mz8001`/`-mz8002` flags; won't build with modern tools |
| ACK (Amsterdam Compiler Kit) | Active (GitHub) | Has Z8000 code generator; builds on modern systems |
| GDB z8k simulator       | Archived         | Can execute Z8000 binaries for testing   |

### Files to Retarget (The Actual Work)

Based on Johnson's "Tour Through the Portable C Compiler," the machine-
dependent files for a Z8001 back-end total approximately 1,600 lines:

**Pass 1 (Frontend, ~600 lines):**

- `macdefs.h`: `SZINT=16`, `SZSHORT=16`, `SZLONG=32`, `SZPOINT=16` (small
  model) or `SZPOINT=32` (large model), alignment rules, register count
- `local.c`: Function prolog/epilog generation, switch statement compilation,
  storage class rewriting for Z8001 addressing modes

**Pass 2 (Backend, ~1,000 lines):**

- `mac2defs.h`: Register names (R0-R15, RR0-RR14), register classes (byte,
  word, long, quad), stack frame layout
- `table.c`: Instruction templates -- the core of the code generator.  Each
  template: shape to match, goal, resources needed, assembly output, rewriting
  rule.  Start from the 8086 back-end and adapt for Z8000 instruction set.
- `local2.c`: Functions to print register names, opcodes, and addresses in
  Z8000 assembly syntax
- `order.c`: Sethi-Ullman number computation for Z8000 register set,
  instruction ordering heuristics.  Johnson warns: "The difficulty of
  [order.c] should not be minimized."


## 9. V7/x86: Nordier's Port Using ACK

### Overview

Robert Nordier ported Unix V7 to the i386 around 1999, revised 2006-2007.
The project used ACK (Amsterdam Compiler Kit) as the C compiler.  The full
V7/x86 distribution is BSD-licensed with source, though the compiler itself
was distributed as binaries only.

- V7/x86 home: https://www.nordier.com/v7x86/ (partially offline, archived)
- Source mirror: https://github.com/calmsacibis995/v7x86
- Internet Archive: https://archive.org/details/unix-v-7-nordier-v-7x-86-0.8a

### No C Syntax Changes Were Needed

ACK has a native K&R C frontend (`cemcom`) that accepted V7's C source
directly.  All V7 C source files retain their original K&R function
declaration syntax -- e.g. `sleep(chan, pri) caddr_t chan;`.  No
K&R-to-ANSI conversion was performed.

### What Nordier Changed in V7

The changes were purely architectural (PDP-11 -> i386), not linguistic:

**Completely rewritten (machine-dependent):**
- `mch.s` -- all-new x86 assembly (context switch, traps, interrupts)
- `machdep.c` -- x86 hardware setup (8253 PIT, CMOS RTC, signal frames)
- `trap.c` -- x86 exception dispatch (GPF, page fault, etc.)
- `reg.h` -- x86 register definitions (EAX-EDI, EIP, EFL)
- `seg.h` -- simplified from PDP-11 segmentation to x86 paging
- All device drivers -- new IDE, floppy, CD-ROM, screen, serial

**Modified headers:**
- `param.h` -- page size (4 KB vs 64 bytes), memory layout, type widths

**Minor tweaks:**
- `main.c`, `sig.c`, `sys1.c`, `subr.c`, `prf.c`

**Untouched (pure algorithmic kernel files):**
- `alloc.c`, `pipe.c`, `text.c`, `slp.c`, `sys4.c`

Files modified by Nordier carry the header:
`/* Changes: Copyright (c) 1999 Robert Nordier. All rights reserved. */`

### What Nordier Changed in ACK

He stripped ACK down to just the C compilation pipeline:

- `em_cemcom` -- K&R C frontend (produces EM intermediate code)
- `em_opt` -- EM peephole optimizer
- `i386cg` -- EM-to-x86 code generator (**modified** to output assembly
  for his custom `asx` assembler instead of ACK's own assembler)

He did not use ACK's assembler, linker, or non-C language frontends.
He wrote:

- A new compiler driver (`ncc.c`), replacing both ACK's and V7's `cc`
- Hand-written x86 assembly for the EM runtime library (`libem`):
  block moves, switch/case support, 8087 floating-point
- His own assembler (`asx`) and used BSD `ld` as the linker

The V7/x86 distribution also included `em_cemcom.ansi` (the ANSI C
frontend), but this was not used for compiling V7 itself -- it was
included so that new code could be written in ANSI C on the running
V7/x86 system.

The source to the modified ACK components (the C frontend, optimizer, and
code generator) was never published -- only FreeBSD/i386 binaries were
distributed.  This is irrelevant for the Z8001 project since those changes
were x86-specific.

### Relevance to the Z8001 Project

Nordier's work proves two things:

1. **ACK compiles V7 C source with zero syntax modifications.**  The K&R C
   frontend handles all V7 idioms natively.

2. **The V7 kernel port requires only machine-dependent changes.**  Pure
   algorithmic files like `alloc.c`, `pipe.c`, `text.c`, `slp.c` needed
   no changes at all.  The work is concentrated in `mch.s`, `machdep.c`,
   `trap.c`, headers, and device drivers -- exactly the files that must
   be rewritten for any new architecture.

The upstream ACK (https://github.com/davidgiven/ack) already has both a
K&R C frontend and a Z8000 code generator back-end.  This makes ACK a
strong candidate for compiling V7 for the Z8001: use the same K&R C
frontend that compiled V7/x86, but target the Z8000 back-end instead of
i386.


## 10. ACK Z8000 Backend Assessment

Source: `/Users/paxia/Projects/PCC/ack/mach/z8000/`

### Overview

The Z8000 code generator was written by Jan Voors at the Vrije Universiteit
Amsterdam (Tanenbaum's group) in 1983.  It targets the **Z8002 (non-segmented)
memory model** with 16-bit pointers (`EM_PSIZE = 2`).

### Register Allocation

| Class    | Registers              | Width  | Purpose                          |
|----------|------------------------|--------|----------------------------------|
| REG      | R0-R12                 | 16-bit | General-purpose                  |
| B2REG    | R0-R7                  | 16-bit | Byte-addressable (low byte)      |
| XREG     | R1-R12                 | 16-bit | Index registers (R0 excluded)    |
| LWREG    | RR0,RR2,RR4,RR6,RR8,RR10 | 32-bit | Register pairs               |
| DLWREG   | RQ0,RQ4,RQ8            | 64-bit | Quad registers (multiply/divide) |

R13 = local base (frame pointer).  RR14 = hardware stack pointer.

### Code Generator Table (1,857 lines)

All 15 EM operation groups are fully covered:

- **Loads/Stores:** All sizes (1, 2, 4, arbitrary).  Peephole patterns for
  combined `lal loi`, `lae loi`, `lal sti` sequences.
- **Integer arithmetic:** 16-bit and 32-bit add, subtract, multiply, divide,
  remainder, negate, shift.  ~60 peephole optimizations for
  increment/decrement, constant shifts, and combined load-operate-store
  patterns.  Uses Z8000-specific `inc`/`dec` with immediate range -16 to +16.
- **Unsigned arithmetic:** Divide and remainder via runtime routines
  (`dvu2`/`dvu4`, `rmu2`/`rmu4`).  Shifts use logical shift (`sdl`/`sdll`).
- **Pointer arithmetic:** Optimized with deferred regconst2 additions.
- **Comparisons:** Heavily optimized (~312 lines).  Fused compare+branch,
  fused compare+test using Z8000's `tcc` instruction.  36 patterns for
  `cmi+test+logical` combinations alone.
- **Branches:** All six signed/unsigned conditionals with 4 pattern variants
  each.  Fused `cmp+branch` and `and+branch` patterns.
- **Procedure calls:** `calr` for direct, `call` indirect.  Results in
  R0 (16-bit), RR0 (32-bit), or R0-R2/RR0-RR2 (48/64-bit).
- **Conversions:** 1->2, 1->4, 2->4, 4->2 using `extsb`/`exts`.
- **Logic/bitwise:** and, or, xor, complement, rotate for 16-bit and
  arbitrary sizes.
- **Sets:** Membership test (`inn`) and singleton creation (`set`) with
  constant-position optimizations.
- **Arrays:** Address, load, and store via runtime.  Peephole for known
  descriptors with shift-based element size calculation.
- **Struct/large objects:** Arbitrary-size loi/sti/blm using `ldir`/`lddr`.

No TODO/FIXME/XXX markers in the code generator table.

### Assembler (mach0.c-mach5.c, ~1,200 lines)

**Covers essentially the entire Z8000 instruction set:**
- All arithmetic, logic, shift, rotate, load/store instructions
- All addressing modes: Register, Immediate, Indirect Register, Direct
  Address, Indexed, Relative (RA), Base Address (BA), Base Index (BX)
- All register types: byte (RH/RL), word (R), long (RR), quad (RQ)
- All condition codes, flags, control registers
- Block operations (ldir/lddr, cpir/cpdr, etc.)
- I/O instructions (in/out, sin/sout, block I/O)
- System instructions (sc, halt, iret, ldctl, ldps)

The assembler supports **segmented addressing modes** even though the code
generator does not currently use them.  Three `???` comments about relocation
information for segmented addresses remain unresolved.

### Runtime Library (libem/, 34 assembly files)

Implements EM operations too complex for inline code:
- Array operations (aar, sar, lar)
- Block move (blm) with overlap-safe ldir/lddr
- Integer conversions (cii, cuu)
- 32-bit signed/unsigned compare (cmi4, cmu4)
- Unsigned divide/remainder (dvu2, dvu4, rmu2, rmu4)
- Case dispatch (csa compact, csb search)
- Trap handling (trp)
- Heap management (strhp)

### What Is Missing

1. **No floating point.**  All 14 FP operations (`adf`, `sbf`, `mlf`, `dvf`,
   `ngf`, `fef`, `fif`, `zrf`, `cfi`, `cif`, `cfu`, `cuf`, `cff`, `cmf`)
   trap with EILLINS (illegal instruction).  `con_float()` emits dummy zeros.
   This is a deliberate choice — the Z8000 has no FPU and no software FP
   library was implemented.  **V7 kernel does not use floating point**, so
   this only affects userland programs (`bc`, `dc`, math library).

2. **Non-reentrant runtime.**  A shared global `saveret`/`savereg` area
   (in `save.s`) is used by `blm`, `dvu2/4`, `rmu2/4`, `sar`, `lar`, `cii`,
   `cms`, `dup`.  These routines are **not interrupt-safe**.  Must be fixed
   for a real OS kernel.

3. **sigtrp.s is empty.**  Signal trap support is unimplemented.

4. **Monitor is bare-metal.**  `libmon/mon.s` provides only read/write/exit
   via `sc` (system call) instructions, targeting a Z8000 development board.
   The write routine has a hardcoded 5000-iteration delay loop.  This must
   be **entirely replaced** with V7 Unix system call stubs.

5. **Segmented addressing modes unused by code generator.**  The assembler
   supports RA/BA/BX modes, but the code generator only emits IR/DA/X/IM.
   For the small-model approach (16-bit pointers, single segment per process)
   this is fine.

6. **con_mult() only handles 4-byte integers.**  Other sizes cause a fatal
   error.


## 11. ACK K&R C Frontend Assessment

Source: `/Users/paxia/Projects/PCC/ack/lang/cem/cemcom/`

### Overview

The K&R C frontend (`cemcom`) is an LLgen-based LL(1) parser from VU
Amsterdam (copyright 1987).  ~80 source files.  An ANSI C frontend
(`cemcom.ansi`) exists as a sibling directory.

### V7 C Idiom Support: Complete

| V7 Idiom                        | How It Is Handled                          |
|----------------------------------|--------------------------------------------|
| Implicit int                     | `ds_notypegiven` flag, defaults to `int_type` |
| Old-style function definitions   | Grammar handles K&R parameter lists; undeclared formals default to int |
| Implicit function declarations   | Undeclared identifier before `(` → `extern int f()` |
| `register` without type          | Implicit int applies uniformly              |
| Enums                            | Fully implemented                          |
| Bitfields                        | Supported (NOBITFIELD not defined)          |
| Typedef                          | Fully implemented                          |
| `asm()` statements               | Supported at top-level and block-level     |
| Built-in preprocessor            | `#define`, `#include`, `#if`, `#ifdef`, `#ifndef`, `#else`, `#endif`, `#line` |
| Float/double                     | Supported; float formals promoted to double |
| Structure assignment             | Supported                                  |

The `-R` option enables "Restricted C" mode that warns about any extensions
beyond what K&R describes.

### What cemcom Does NOT Support (ANSI features)

- `const`, `volatile`, `signed` keywords
- Function prototypes
- `//` comments
- String literal concatenation
- Trigraphs

These are all irrelevant for V7 source code.

### Build Status: Not Currently Built

The modern ACK build system (`build.py`) only builds `cemcom.ansi`.  The
K&R frontend source is present and has been kept compilable through
modernization commits, but there is no `build.py` for it.

**This probably does not matter:**  the ANSI frontend (`cemcom.ansi`) accepts
K&R C as a superset.  V7 code can be compiled with either frontend.  The
ANSI frontend is the one that actually builds today.


## 12. Conclusions: Compiler Choice for V7-to-Z8001 Port

### Option A: ACK (Recommended)

Use the upstream ACK from https://github.com/davidgiven/ack

Advantages:
- **ANSI C frontend compiles V7 K&R C unchanged** (proven by Nordier's
  V7/x86 port — zero syntax modifications needed)
- **Z8000 code generator already exists** — 1,857-line production-quality
  backend with extensive optimizations
- **Assembler supports full Z8000 ISA** including segmented modes
- **Builds on modern systems** (macOS, Linux)
- **Actively maintained** by David Given
- The Z8002 (non-segmented, 16-bit pointer) model matches the recommended
  small-model approach for the initial port

Work needed:
- Replace `libmon/` with V7 Unix system call stubs for Z8001
- Fix non-reentrant runtime routines for interrupt safety
- Add software floating-point library for userland (not needed for kernel)
- Write `build.py` for cemcom if strict K&R semantics are desired (or just
  use cemcom.ansi)

### Option B: Retarget V7's Own PCC

Use the PCC from the V7 source tree (`/usr/src/cmd/mip/` + `/usr/src/cmd/cc/`)

Advantages:
- Historically authentic (exactly how the Interdata and VAX ports were done)
- Runs on V7 itself (cross-compile on PDP-11 emulator)
- The MIT back-ends (8086, 68000, etc.) serve as retargeting templates

Work needed:
- Write ~1,600 lines of machine-dependent files for Z8001 (macdefs.h,
  mac2defs.h, table.c, local.c, local2.c, order.c)
- Write or adapt a Z8001 cross-assembler
- Write a cross-linker
- All development constrained to PDP-11 memory limits (painful)

### Recommendation

**Start with ACK.**  The Z8000 backend is already written and the compiler
runs on modern hardware.  Cross-compile V7 for Z8001 on your Mac, test on
a Z8000 emulator, iterate quickly.  The PCC retargeting approach is
historically authentic but unnecessarily painful for practical work.


## References

- S. C. Johnson, "A Tour Through the Portable C Compiler" (V7 Unix manual, Volume 2)
  - HTML: https://wolfram.schneider.org/bsd/7thEdManVol2/porttour/porttour.html
  - PDF: https://c9x.me/compile/bib/pcc-tour.pdf
- S. C. Johnson, "A Portable Compiler: Theory and Practice" (1978 ACM POPL)
- Alan Snyder, "A Portable Compiler for the Language C" (MIT-LCS-TR-149, 1975)
  - https://dspace.mit.edu/handle/1721.1/149444
- Portable C Compiler, Wikipedia
  - https://en.wikipedia.org/wiki/Portable_C_Compiler
- Stephen C. Johnson, Wikipedia
  - https://en.wikipedia.org/wiki/Stephen_C._Johnson
- Stephen Curtis Johnson: Geek of the Week (Simple Talk interview)
  - https://www.red-gate.com/simple-talk/opinion/geek-of-the-week/stephen-curtis-johnson-geek-of-the-week/
- PCC Project Homepage (may be offline)
  - http://pcc.ludd.ltu.se/
- Anders Magnusson, "Bringing PCC into the 21st Century" (OpenBSD presentation)
  - https://www.openbsd.org/papers/magnusson_pcc.pdf
- A History of C Compilers (The Chip Letter)
  - https://thechipletter.substack.com/p/a-history-of-c-compilers-part-1-performance
- BSD Fund: PCC Fund
  - https://bsdfund.org/projects/pcc/
- On the trail of PCC for the 8086 (Virtually Fun)
  - https://virtuallyfun.com/2022/11/04/on-the-trail-of-pcc-for-the-8086/
- Onyx Systems, Wikipedia
  - https://en.wikipedia.org/wiki/Onyx_Systems
- Zilog System 8000 (Stuttgart Computer Museum)
  - https://computermuseum.informatik.uni-stuttgart.de/dev_en/s8000/s8000.html
- Commodore 900, Wikipedia
  - https://en.wikipedia.org/wiki/Commodore_900
- P8000 / WEGA, Wikipedia
  - https://en.wikipedia.org/wiki/P8000
- TUHS: Unix on Zilog Z8000 thread
  - https://inbox.vuxu.org/tuhs/3E0694EF-1742-4313-BA34-D4D386FF6942@planet.nl/T/
- Zilog Z8000, Wikipedia
  - https://en.wikipedia.org/wiki/Zilog_Z8000
- GNU as Z8000 Directives
  - https://sourceware.org/binutils/docs/as/Z8000-Directives.html
- GCC z8k backend (Cygnus, archived)
  - https://github.com/z8k/z8k-gcc
- Amsterdam Compiler Kit (ACK)
  - https://github.com/davidgiven/ack
- Z8000 Software (kranenborg.org)
  - https://www.kranenborg.org/z8000/software/index.htm
- Portability Proposal Memo (Bell Labs, Ritchie)
  - https://www.bell-labs.com/usr/dmr/www/firstport.html
- 32V Report (Bell Labs)
  - https://www.nokia.com/bell-labs/about/dennis-m-ritchie/otherports/32v.html
- knrcc -- preserved V7 K&R C compiler
  - https://github.com/AoiMoe/knrcc
- v7unix/v7unix -- V7 userland modernization project
  - https://github.com/v7unix/v7unix
- V7/x86 (Robert Nordier)
  - https://www.nordier.com/
- Modern PDP-11 C Compilers (Retro Computing Forum)
  - https://retrocomputingforum.com/t/modern-pdp-11-c-compilers/2329
- calmsacibis995/v7x86 (V7/x86 source mirror)
  - https://github.com/calmsacibis995/v7x86
- V7/x86 on Internet Archive
  - https://archive.org/details/unix-v-7-nordier-v-7x-86-0.8a
- Nordier ncc page (archived, Wayback Machine)
  - https://web.archive.org/web/2016/http://www.nordier.com/software/ncc.html
- ACK CEM Reference Manual
  - https://tack.sourceforge.net/olddocs/crefman.html
- ACK at VU Amsterdam
  - https://www.cs.vu.nl/~ceriel/ack/index.html
