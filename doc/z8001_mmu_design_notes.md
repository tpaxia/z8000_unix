# Z8001-Based Paged MMU Design Notes

## Background: The Onyx C8002 and Its Custom MMU

The Onyx C8002 (1980) was one of the earliest microprocessor-based Unix systems. Rather than using the segmented Z8001 with Zilog's Z8010 MMU, Onyx chose the non-segmented Z8002 and built their own custom Memory Management Controller (MMC). This was a pragmatic decision — the Z8001 and Z8010 were delayed, and Onyx needed to ship.

### Onyx MMC Specifications

- **16 independent map sets**, each consisting of an instruction map and a data map (split I/D)
- **32 page registers per map**, each mapping a 2KB virtual page to a physical page
- **20-bit physical address output**, allowing access to up to 1MB of physical memory
- **Programmed via I/O instructions** to I/O address space
- Described as a "16 segment split I/D 16-bit MMU" with architecture similar to the PDP-11/70

### Why 2KB Pages?

The Z8002 has a 16-bit (64KB) address space. With 2KB pages, each map has 32 entries covering exactly 64KB. This gives fine allocation granularity in a system where total RAM is 256KB–512KB shared among multiple users, and each process is limited to 64KB+64KB (I+D). Larger pages would cause unacceptable internal fragmentation at these sizes.

### Context Switching

With 16 map sets in hardware, up to 15 user processes (plus the kernel) could have their maps resident simultaneously. Context switching between these processes required only writing a map-set selector — no page register reloading. If more than 16 processes were active, the kernel would save/restore a full map set (64 register writes via I/O) for overflow cases. On a 4–8 user timesharing system, this was rarely needed.

### Software

The ONIX operating system was a port of V7 Unix. Only about 60 lines of V7 C code were modified; all machine-dependent support was new assembly code and drivers. The architecture was deliberately PDP-11-like to make porting straightforward. System III was later ported as well.

---

## Proposed Design: Using Z8001 Segmentation to Select MMU Map Sets

> **What exists today.** The emulated machine (`emu/test_driver.cpp`) implements the core of this design: the segment number selects one of 128 maps of 32 pages of 2 KB. It does not implement split instruction/data maps, per-page protection bits or the normal-mode segment check described below, and only two page pairs of the kernel's segment are ever remapped (the u-area and a copy window). See `kernel-technical-reference.md`, Paged MMU.

### Core Concept

Instead of using the Z8001's segmentation in its intended manner (per-segment base+limit translation via the Z8010), repurpose the 7-bit segment number as a **map set selector** for an external paged MMU. The segment number, which the Z8001 places on the address bus with every memory access, tells the MMU which set of page registers to use. Within each map set, the 16-bit offset is translated through page registers, exactly as the Onyx MMC did.

This combines the advantages of both approaches:

- **Hardware-assisted map set selection** — the segment number is embedded in the PC, SP, and all pointers, so the correct map set is automatically active whenever the CPU executes a given process's code. No explicit I/O write needed to switch maps.
- **Fine-grained paged translation** — non-contiguous physical pages can be assembled into a contiguous virtual address space, eliminating the fragmentation problems of the Z8010's base+limit model.

### Architecture

```
Z8001 Bus Cycle:
  [7-bit segment] [16-bit offset] [ST0-ST3 status]

MMU operation:
  1. Segment number (7 bits) → selects one of N map sets
  2. ST lines → distinguish instruction fetch vs data access (split I/D)
  3. Upper 5 bits of offset → select page register within map (32 × 2KB pages)
  4. Page register supplies physical frame number
  5. Lower 11 bits of offset pass through unchanged
  6. Result: physical address = [frame number][offset within page]
```

### Map Set Sizing

Using 5–6 bits of the segment number gives 32–64 map sets. Each map set contains:

- 32 instruction page registers (I map)
- 32 data page registers (D map)
- Permission/valid bits per entry

This supports 31–63 user process contexts plus the kernel, with zero-cost context switching between all of them.

### Protection Model

The Z8001 outputs system/normal mode status on every bus cycle. The MMU uses this for a simple but effective protection scheme:

| Mode | Segment | Action |
|------|---------|--------|
| System | Any | Access permitted — MMU translates through the addressed segment's map set |
| Normal | Own segment | Access permitted — MMU translates normally |
| Normal | Other segment | **Trap** — process attempting to access another process's address space |

Implementation requires:

- A register holding the current process's segment number
- A comparator checking the bus segment number against the stored value
- A gate conditioned on the system/normal status line
- If normal mode and segment mismatch → assert trap/interrupt

This is a handful of TTL chips at most.

### Per-Page Protection

Each page register entry can include protection bits:

- **Valid bit** — page is mapped; if clear, access traps (for catching wild pointers and limiting process size)
- **Write-protect bit** — allows read-only text segments, shared libraries
- **System-only bit** — optional additional per-page restriction

The MMU checks these on every access and traps on violations.

### Kernel Access to User Memory

This design provides an elegant solution to the copyin/copyout problem (kernel reading/writing user process memory during system calls like `read()` and `write()`):

- In **system mode**, the CPU can use any segment number freely. The MMU translates through whatever map set that segment selects.
- The kernel accesses user process N's memory simply by constructing a pointer with segment number N.
- No special instructions needed (unlike PDP-11's `MTPD`/`MFPD`).
- No map register swapping or temporary remapping needed.
- The kernel can even access **multiple processes' memory simultaneously** — e.g., reading from segment 5 and writing to segment 7 for a pipe transfer, with no map switching at all.

This is cleaner than both the PDP-11 approach (special previous-space instructions) and the Onyx approach (which would have required I/O writes to change the active map set).

### Split I/D Space

The Z8001 outputs status signals (ST0–ST3) that distinguish instruction fetches from data accesses on every bus cycle. The MMU uses this to select the I map or D map within the current map set. This gives each process a full 64KB instruction space and 64KB data space, identical to the PDP-11/70 split I/D model and the Onyx MMC.

### Hardware Implementation

The MMU can be built from:

- **SRAM** for page register storage — addressed by {segment number, I/D select, page number within offset}
- **Output latches** for the physical frame number
- **Comparator + gate logic** for the protection check (segment match in normal mode)
- **Decode logic** for CPU I/O writes to load page registers

The SRAM approach is the same one used by Onyx for the C8002 and by Plexus Computer for their later P60 (68000-based) system. The total hardware complexity is modest — the entire MMU could fit on a moderate-sized board with standard 74-series logic and a few SRAMs.

### Comparison

| Feature | PDP-11/70 | Onyx C8002 MMC | This Design |
|---------|-----------|----------------|-------------|
| Map sets | 3 (kernel/supervisor/user) | 16 | 32–64 |
| Pages per map | 8 (8KB pages) | 32 (2KB pages) | 32 (2KB pages) |
| Split I/D | Yes | Yes | Yes |
| Context switch cost | Reload user maps | Write map-set selector | Free (segment in PC) |
| Kernel→user access | Special instructions (MTPD/MFPD) | I/O write to switch map | Direct via segment number |
| Physical address bits | 22 (4MB) | 20 (1MB) | Configurable |
| Hardware | On-chip | Custom board logic | Custom board logic |

---

## References and Resources

### Primary Documentation (bitsavers.org, /pdf/onyx/c8002/)

- `C8002-User-Guide.pdf` — 46-page manual (July 1980 draft), most likely source of MMC register-level detail
- `Onyx_C8002_Brochure.pdf` — contains MMC description on page 6
- `Onyx_Diagnostic_Monitor.pdf` — may contain MMU diagnostic/test information
- `ONIX_1.4_Release_Notice_Mar81.pdf`
- `UNIX_3.0.3_Software_Release_Notice_May83.pdf`
- Board photographs: `z8000_top.jpg`, `z8000_bot.jpg`

### Discussion

- TUHS mailing list thread, January 2020: "Unix on Zilog Z8000?" — includes firsthand accounts from Clem Cole, Mary Ann Horton, and analysis by Derek Fawcus
- `Onyx_History.txt` (bitsavers, /pdf/plexus/history/) — detailed account by John Bass of the V7 port and Onyx history

### Related Systems

- **Plexus P35/P40** — Z8000-based Unix systems built by ex-Onyx engineers (~1982)
- **Plexus P60** — 68000-based, used SRAM-based MMU (designed by Jonathan Lundell)
- **Zilog Z8010 MMU** — Zilog's official segmented MMU for the Z8001 (base+limit, not paged)
- **Zilog Z8015 PMMU** — later paged MMU for Z8003/Z8004 virtual memory processors
