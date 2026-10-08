// Test Driver for Z8001 Kernel Bring-up
//
// Loads ROM (segment 0), kernel (segment 1), and C handler into an 8MB
// Z8001 address space, runs the CPU, and verifies that the kernel boots,
// mounts the filesystem, opens /dev/console, prints "Z8000 Unix",
// forks process 1 which exec's /etc/init from the filesystem, and
// init writes "hello from exec" via syscall and exits.
//
// The I/O port space includes a DMA controller for the RAM disk driver.
//
// Usage: ./test_driver [-t] [-r] [-m] [-c cycles] [-d hd-image] [-i console-input] [-x expected-text]
//   -t  Enable instruction tracing
//   -r  Enable register tracing
//   -m  Enable memory tracing
//   -w text -I input  Type a second input after text appears, plus 100 ticks
//   -n ticks -M text  Measure clock delivery after text (default: shell prompt)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <queue>
#include <getopt.h>
#include <z8000/z8000.h>
#include "memory.h"
#include "../v7z8000/usr/sys/machine/mmu.h"

// Observe the existing request latch without changing interrupt delivery.
class ClockObservedCPU : public z8001_device {
public:
    bool clock_pending() const { return (m_irq_req & Z8000_NVI) != 0; }
    uint64_t clock_accepted = 0;
    void access_fault() { m_irq_req |= Z8000_SEGTRAP; }
    void set_opcode_bus(z8000_memory_bus *bus) { m_opcache.bus = bus; }
protected:
    uint16_t GET_FCW(uint32_t vec) override {
        // Count actual NVI dispatches, independently of the latch accounting.
        if (vec == PSA_ADDR() + m_vector_mult * 0x18) clock_accepted++;
        return z8001_device::GET_FCW(vec);
    }
};

// Paged MMU: 128 segments x 32 pages x 2KB pages
// System/service banks start identity-mapped; user banks start unmapped.
class MMU : public z8000_memory_bus {
public:
    MMU(MemoryRegion *phys, unsigned ram_frames)
        : m_phys(phys), m_ram_frames(ram_frames), m_trace(false) {
        // Bootstrap mappings; the kernel installs user page tables on allocation.
        for (int seg = 0; seg < 128; seg++) {
            m_iseg[seg] = seg;
            m_stack_base[seg] = 0xffff;
            for (int page = 0; page < 32; page++) {
                m_pages[seg][page] = (seg <= 1 || seg == 127) ? seg * 32 + page : 0xffff;
                m_attr[seg][page] = (seg <= 1 || seg == 127) ? MM_SYS : 0;
            }
        }
        set_upage(62);
    }

    // Test-only denied bus access. The normal page attributes stay unchanged.
    void set_fault(ClockObservedCPU *cpu, char kind, unsigned offset) {
        m_cpu = cpu; m_fault_kind = kind; m_fault_offset = offset;
    }
    void arm_fault() { m_fault_armed = true; }
    unsigned fault_count = 0;
    unsigned absent_count = 0;
    unsigned unmapped_count = 0;
    unsigned protection_count = 0, warning_count = 0, shared_text_peak = 0;
    unsigned ram_frames() const { return m_ram_frames; }

    // First-word fetch is externally visible as bus status 1101 (manual 2.3.4).
    // These latches contain bus evidence, never a hidden CPU register snapshot.
    void first_word(uint32_t address) { m_first_word = address; }
    void latch(uint32_t address, unsigned size, unsigned reason) {
        uint16_t seg = (address >> 16) & 127, lo = address & 65535;
        uint16_t hi = lo + size - 1;
        if (!m_fault_status) {
            m_fault_seg = seg; m_fault_lo = lo; m_fault_hi = hi;
            m_fault_pc = m_first_word;
        } else {
            if (seg != m_fault_seg || m_first_word != m_fault_pc) reason |= MF_MIXED;
            if (lo < m_fault_lo) m_fault_lo = lo;
            if (hi > m_fault_hi) m_fault_hi = hi;
        }
        m_fault_status |= MF_VALID | reason;
        if (m_cpu) m_cpu->access_fault();
    }
    uint16_t fault_register(unsigned port) const {
        switch (port) {
        case MM_FAULT: return m_fault_status;
        case MM_FSEG: return m_fault_seg;
        case MM_FLOW: return m_fault_lo;
        case MM_FHIGH: return m_fault_hi;
        case MM_PCSEG: return (m_fault_pc >> 16) & 127;
        case MM_PC: return m_fault_pc & 65535;
        }
        return 0;
    }
    void acknowledge() { m_fault_status = 0; }
    void stack_select(uint16_t seg) { m_stack_select = seg & 127; }
    void stack_base(uint16_t base) { m_stack_base[m_stack_select] = base; }

    bool absent(uint32_t addr, unsigned size, bool writing = false, bool program = false) {
        unsigned reason = writing ? MF_WRITE : MF_READ;
        if (program) reason |= MF_FETCH;
        uint32_t physical = translate(addr);
        unsigned seg = (addr >> 16) & 127, page = (addr >> 11) & 31;
        bool system = !m_cpu || (m_cpu->get_fcw() & 0x4000);
        if ((physical >> 11) == 0xffff) {
            unmapped_count++;
            latch(addr, size, reason | MF_UNMAP);
            return true;
        }
        if ((writing && (m_attr[seg][page] & MM_RO)) ||
            (!system && (m_attr[seg][page] & MM_SYS))) {
            protection_count++;
            latch(addr, size, reason | MF_PROT);
            return true;
        }
        // The EPU engine has dedicated storage, independent of low RAM.
        if (!((physical >= 0x7f0000 && physical + size <= 0x800000) ||
            physical + size <= m_ram_frames * 2048)) {
            absent_count++;
            latch(addr, size, reason | MF_PROT);
            return true;
        }
        if (!system && writing && !program && m_stack_base[seg] != 0xffff &&
            (addr & 65535) >= m_stack_base[seg] &&
            (addr & 65535) < unsigned(m_stack_base[seg]) + 256) {
            warning_count++;
            latch(addr, size, reason | MF_WARN); // Store succeeds; no replay needed.
        }
        return false;
    }

    bool denied(uint32_t addr, unsigned size, bool writing) {
        if (!m_fault_armed || !m_cpu) return false;
        unsigned seg = (addr >> 16) & 0x7f, off = addr & 0xffff;
        if (seg <= 1 || seg == 127 || off > m_fault_offset ||
            off + size <= m_fault_offset) return false;
        unsigned fcw = m_cpu->get_fcw();
        if (m_fault_kind == 'u') {
            if (fcw & 0x4000) return false;
        } else {
            if ((fcw & 0xc000) != 0xc000 || writing != (m_fault_kind == 'w'))
                return false;
        }
        fault_count++;
        latch(addr, size, (writing ? MF_WRITE : MF_READ) | MF_PROT);
        return true;  // Suppress the failed write/read, like an external MMU.
    }

    void set_trace(bool enable) { m_trace = enable; }

    // KDSA6 equivalent: remap seg1 pages 30-31 (virtual 0xF000-0xFFFF)
    void set_upage(uint16_t frame) {
        m_pages[1][30] = frame;
        m_pages[1][31] = frame + 1;
        // Software EPU service uses the same per-process system stack.
        m_pages[127][30] = frame;
        m_pages[127][31] = frame + 1;
        if (m_trace)
            printf("  MMU: UPAGE=%d (seg1 pages 30-31 -> frames %d,%d)\n",
                   frame, frame, frame + 1);
    }

    // Copy window: remap seg1 pages 28-29 (virtual 0xE000-0xEFFF)
    void set_wpage(uint16_t frame) {
        m_pages[1][28] = frame;
        m_pages[1][29] = frame + 1;
        if (m_trace)
            printf("  MMU: WPAGE=%d (seg1 pages 28-29 -> frames %d,%d)\n",
                   frame, frame, frame + 1);
    }

    // Instruction map selects a backing bank; data/stack retain their map.
    void set_imap(uint16_t value) {
        m_iseg[(value >> 8) & 0x7f] = value & 0x7f;
    }

    // General page-table access. Select and frame writes are separate bus cycles.
    void select_page(uint16_t value) { m_page_select = value & 4095; }
    void set_page(uint16_t frame) {
        m_attr[m_page_select >> 5][m_page_select & 31] = frame & (MM_RO|MM_SYS);
        m_pages[m_page_select >> 5][m_page_select & 31] = frame == 0xffff ? frame : frame & ~(MM_RO|MM_SYS);
        if (frame != 0xffff && (frame & MM_RO) && !(frame & MM_SYS)) {
            unsigned owners = 0;
            for (unsigned seg = 2; seg < 127; seg++)
                if ((m_attr[seg][m_page_select & 31] & MM_RO) &&
                    m_pages[seg][m_page_select & 31] == (frame & ~(MM_RO|MM_SYS))) owners++;
            if (owners > shared_text_peak) shared_text_peak = owners;
        }
    }

    uint32_t instruction_address(uint32_t addr) const {
        return (uint32_t(m_iseg[(addr >> 16) & 0x7f]) << 16) | (addr & 0xffff);
    }

    uint32_t translate(uint32_t addr) {
        uint32_t seg = (addr >> 16) & 0x7F;
        uint32_t offset = addr & 0xFFFF;
        uint32_t page = offset >> 11;
        uint32_t pg_off = offset & 0x7FF;
        uint32_t frame = m_pages[seg][page];
        return (frame << 11) | pg_off;
    }

    u8 read_byte(u32 addr) override {
        return (absent(addr, 1) || denied(addr, 1, false)) ? 0 : m_phys->read_byte(translate(addr));
    }

    u16 read_word(u32 addr) override {
        return (absent(addr, 2) || denied(addr, 2, false)) ? 0 : m_phys->read_word(translate(addr));
    }

    void write_byte(u32 addr, u8 val) override {
        if (!absent(addr, 1, true) && !denied(addr, 1, true)) m_phys->write_byte(translate(addr), val);
    }

    void write_word(u32 addr, u16 val) override {
        if (!absent(addr, 2, true) && !denied(addr, 2, true)) m_phys->write_word(translate(addr), val);
    }

    void write_word(u32 addr, u16 val, u16 mask) override {
        if (!absent(addr, 2, true) && !denied(addr, 2, true)) m_phys->write_word(translate(addr), val, mask);
    }

    u8 program_byte(u32 addr) {
        return (absent(addr, 1, false, true) || denied(addr, 1, false)) ? 0 : m_phys->read_byte(translate(addr));
    }
    u16 program_word(u32 addr) {
        return (absent(addr, 2, false, true) || denied(addr, 2, false)) ? 0 : m_phys->read_word(translate(addr));
    }
private:
    uint16_t m_fault_status = 0, m_fault_seg = 0, m_fault_lo = 0, m_fault_hi = 0;
    uint32_t m_first_word = 0, m_fault_pc = 0;
    uint16_t m_stack_select = 0, m_stack_base[128];
    uint16_t m_attr[128][32];
    ClockObservedCPU *m_cpu = nullptr;
    bool m_fault_armed = false;
    char m_fault_kind = 0;
    unsigned m_fault_offset = 0;
    MemoryRegion *m_phys;
    unsigned m_ram_frames;
    uint16_t m_page_select = 0;
    uint16_t m_pages[128][32];
    uint8_t m_iseg[128];
    bool m_trace;
};

#include "user_profile.h"

// All instruction-space accesses, including operands and PC-relative loads.
class InstructionBus : public z8000_memory_bus {
public:
    explicit InstructionBus(MMU &mmu) : m_mmu(mmu) {}
    UserProfile *profile = nullptr;
    u8 read_byte(u32 a) override { return m_mmu.program_byte(m_mmu.instruction_address(a)); }
    u16 read_word(u32 a) override {
        if (profile) profile->sample(a);
        return m_mmu.program_word(m_mmu.instruction_address(a));
    }
    void write_byte(u32 a, u8 v) override { m_mmu.write_byte(m_mmu.instruction_address(a), v); }
    void write_word(u32 a, u16 v) override { m_mmu.write_word(m_mmu.instruction_address(a), v); }
    void write_word(u32 a, u16 v, u16 mask) override {
        m_mmu.write_word(m_mmu.instruction_address(a), v, mask);
    }
protected:
    MMU &m_mmu;
};

class FirstWordBus : public InstructionBus {
public:
    explicit FirstWordBus(MMU &mmu) : InstructionBus(mmu) {}
    u16 read_word(u32 a) override { m_mmu.first_word(a); return InstructionBus::read_word(a); }
};

// Extended IOPorts with DMA controller for RAM disk
class KernelIOPorts : public z8000_io_bus {
public:
    KernelIOPorts(MemoryRegion *mem, MMU *mmu, z8001_device *cpu)
        : m_trace(false), m_memory(mem), m_mmu(mmu), m_cpu(cpu),
          m_dma_blk_hi(0), m_dma_blk_lo(0),
          m_dma_addr_hi(0), m_dma_addr_lo(0),
          m_dma_status(0),
          m_ata_sc(0), m_ata_sn(0), m_ata_cl(0), m_ata_ch(0), m_ata_dh(0),
          m_ata_status(0x40), m_ata_error(0),
          m_ata_buf_idx(0), m_ata_active(false), m_ata_writing(false)
    {
        memset(m_ata_buf, 0, sizeof(m_ata_buf));
    }

    const std::string& console_output() const { return m_console_buf; }
    void set_trace(bool enable) { m_trace = enable; }

    void queue_console_char(uint8_t c) {
        m_console_rx.push(c);
        m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
    }

    bool load_disk(const char *filename) {
        FILE *f = fopen(filename, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open disk image '%s'\n", filename);
            return false;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        m_disk.resize(size);
        size_t nread = fread(m_disk.data(), 1, size, f);
        fclose(f);
        if ((long)nread != size) {
            fprintf(stderr, "Error: Short read on disk image '%s'\n", filename);
            return false;
        }
        printf("  Loaded disk image %-11s (%ld bytes, %ld blocks)\n",
               filename, size, size / 512);
        return true;
    }

    unsigned swap_reads = 0, swap_writes = 0;
    uint64_t swap_delay = 0;
    char swap_fail_kind = 0;
    unsigned swap_fail_nth = 0, swap_errors = 0, swap_user_samples = 0;
    void poll_swap() {
        if (!m_swap_irq_due) return;
        if (!(m_cpu->get_fcw() & 0x4000)) swap_user_samples++;
        if (m_cpu->get_cycles() >= m_swap_irq_due) {
            m_swap_irq_due = 0;
            m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
        }
    }
    void swap_size(unsigned kib) { m_swap.resize(kib * 1024); }
    bool load_hd(const char *filename) {
        FILE *f = fopen(filename, "rb");
        if (!f) {
            fprintf(stderr, "Error: Cannot open HD image '%s'\n", filename);
            return false;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        m_hd.resize(size);
        size_t nread = fread(m_hd.data(), 1, size, f);
        fclose(f);
        if ((long)nread != size) {
            fprintf(stderr, "Error: Short read on HD image '%s'\n", filename);
            return false;
        }
        printf("  Loaded HD image   %-11s (%ld bytes, %ld blocks)\n",
               filename, size, size / 512);
        return true;
    }

    bool save_hd(const char *filename) {
        FILE *f = fopen(filename, "wb");
        if (!f) return false;
        bool ok = fwrite(m_hd.data(), 1, m_hd.size(), f) == m_hd.size();
        return fclose(f) == 0 && ok;
    }

    // z8000_io_bus interface
    u8 read_byte(u16 addr, int mode) override {
        u8 val;
        if (mode == 0) {
            switch (addr) {
                case 0x00E5:  // DMA status
                    val = m_dma_status;
                    break;
                case 0x00F0:
                    // Console RX data: dequeue from FIFO
                    if (!m_console_rx.empty()) {
                        val = m_console_rx.front();
                        m_console_rx.pop();
                        // The interrupt request is a one-bit latch shared by
                        // every VI source, so two characters queued before the
                        // handler runs (or a character and a disk completion)
                        // raise only one interrupt, and the handler reads one
                        // character per interrupt. Keep requesting while data
                        // remains, as a receiver with a FIFO does.
                        if (!m_console_rx.empty())
                            m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
                    } else {
                        val = 0x00;
                    }
                    break;
                case 0x00F2:
                    // Console status: bit 0=TX ready (always), bit 1=RX ready
                    val = 0x01 | (m_console_rx.empty() ? 0x00 : 0x02);
                    break;
                case 0x01F1:  // ATA ERROR
                    val = m_ata_error;
                    break;
                case 0x01F7:  // ATA STATUS
                    val = m_ata_status;
                    break;
                default:
                    val = 0xDE;
                    break;
            }
        } else {
            val = 0xBE;
        }
        if (m_trace) {
            printf("  %sI/O RD8  [%04X] -> %02X\n", mode ? "S" : "", addr, val);
        }
        return val;
    }

    u16 read_word(u16 addr, int mode) override {
        addr &= 0xFFFE;
        u16 val = 0xDEAD;
        if (mode == 0 && addr >= MM_FAULT && addr <= MM_PC)
            return m_mmu->fault_register(addr);
        if (mode == 0 && addr == MM_SWAPSIZE) return m_swap.size()/512;
        if (mode == 0 && addr == 0x00BA)
            val = m_mmu->ram_frames();
        if (mode == 0 && addr == 0x01F0 && m_ata_active && !m_ata_writing) {
            // ATA DATA read: return next word from sector buffer (big-endian)
            if (m_ata_buf_idx < 256) {
                unsigned off = m_ata_buf_idx * 2;
                val = ((u16)m_ata_buf[off] << 8) | m_ata_buf[off + 1];
                m_ata_buf_idx++;
                if (m_ata_buf_idx >= 256) {
                    m_ata_active = false;
                    m_ata_status = 0x40;  // DRDY, clear DRQ
                }
            }
        }
        if (m_trace) {
            printf("  %sI/O RD16 [%04X] -> %04X\n", mode ? "S" : "", addr, val);
        }
        return val;
    }

    void write_byte(u16 addr, u8 val, int mode) override {
        if (m_trace) {
            printf("  %sI/O WR8  [%04X] <- %02X\n", mode ? "S" : "", addr, val);
        }
        if (mode != 0) return;

        switch (addr) {
            case 0x00E0:  // DMA block number high byte
                m_dma_blk_hi = val;
                break;
            case 0x00E1:  // DMA block number low byte
                m_dma_blk_lo = val;
                break;
            case 0x00E2:  // DMA address high byte
                m_dma_addr_hi = val;
                break;
            case 0x00E3:  // DMA address low byte
                m_dma_addr_lo = val;
                break;
            case 0x00E4:  // DMA command
                do_dma(val);
                break;
            case 0x00F0:  // Console TX
                putchar(val);
                fflush(stdout);
                m_console_buf += static_cast<char>(val);
                break;
            case 0x01F2:  // ATA SC
                m_ata_sc = val;
                break;
            case 0x01F3:  // ATA SN (LBA[0:7])
                m_ata_sn = val;
                break;
            case 0x01F4:  // ATA CL (LBA[8:15])
                m_ata_cl = val;
                break;
            case 0x01F5:  // ATA CH (LBA[16:23])
                m_ata_ch = val;
                break;
            case 0x01F6:  // ATA DH (LBA[24:27] + flags)
                m_ata_dh = val;
                break;
            case 0x01F7:  // ATA CMD
                do_ata_cmd(val);
                break;
        }
    }

    void write_word(u16 addr, u16 val, int mode) override {
        addr &= 0xFFFE;
        if (m_trace) {
            printf("  %sI/O WR16 [%04X] <- %04X\n", mode ? "S" : "", addr, val);
        }
        if (mode != 0) return;

        switch (addr) {
            case MM_ACK: m_mmu->acknowledge(); break;
            case MM_STACKSEL: m_mmu->stack_select(val); break;
            case MM_STACKBASE: m_mmu->stack_base(val); break;
            case 0x00BC:  // PAGESEL: 7-bit logical segment, 5-bit page
                m_mmu->select_page(val);
                break;
            case 0x00BE:  // PAGEFRAME: physical frame; 0xffff leaves it unmapped
                m_mmu->set_page(val);
                break;
            case 0x00B8:  // IMAP: logical segment in high byte, I backing bank in low
                m_mmu->set_imap(val);
                break;
            case 0x00B0:  // UPAGE: KDSA6 equivalent
                m_mmu->set_upage(val);
                break;
            case 0x00B4:  // WPAGE: copy window
                m_mmu->set_wpage(val);
                break;
            case 0x01F0:  // ATA DATA write
                if (m_ata_active && m_ata_writing && m_ata_buf_idx < 256) {
                    unsigned off = m_ata_buf_idx * 2;
                    m_ata_buf[off]     = (val >> 8) & 0xFF;  // big-endian
                    m_ata_buf[off + 1] = val & 0xFF;
                    m_ata_buf_idx++;
                    if (m_ata_buf_idx >= 256) {
                        ata_flush_write();
                    }
                }
                break;
        }
    }

private:
    void do_dma(u8 cmd) {
        unsigned blkno = ((unsigned)m_dma_blk_hi << 8) | m_dma_blk_lo;
        unsigned dma_addr = ((unsigned)m_dma_addr_hi << 8) | m_dma_addr_lo;
        unsigned disk_off = blkno * 512;
        // Memory address: segment 1 base (0x010000) + dma_addr
        uint32_t mem_addr = 0x010000 + dma_addr;

        if (cmd == 1) {
            // Read: disk -> memory
            // Zero-fill reads beyond end of disk image (unallocated blocks)
            uint8_t block[512];
            memset(block, 0, 512);
            if (disk_off < m_disk.size()) {
                size_t avail = m_disk.size() - disk_off;
                if (avail > 512) avail = 512;
                memcpy(block, m_disk.data() + disk_off, avail);
            }
            if (!m_memory->load(mem_addr, block, 512)) {
                fprintf(stderr, "DMA read: memory load failed at 0x%06X\n", mem_addr);
                m_dma_status = 0xFF;
                return;
            }
            m_dma_status = 0;
        } else if (cmd == 2) {
            // Write: memory -> disk
            if (disk_off + 512 > m_disk.size()) {
                m_disk.resize(disk_off + 512, 0);
            }
            for (unsigned i = 0; i < 512; i++) {
                m_disk[disk_off + i] = m_memory->read_byte(mem_addr + i);
            }
            m_dma_status = 0;
        } else {
            m_dma_status = 0xFF;
        }
    }

    void do_ata_cmd(u8 cmd) {
        auto &disk = (m_ata_dh & 0x10) ? m_swap : m_hd;
        unsigned lba = m_ata_sn | ((unsigned)m_ata_cl << 8);
        unsigned disk_off = lba * 512;

        bool inject = false;
        if (m_ata_dh & 0x10) {
            unsigned nth = cmd == 0x20 ? ++m_swap_read_cmds : ++m_swap_write_cmds;
            inject = nth == swap_fail_nth &&
                (cmd == 0x20 ? swap_fail_kind == 'r' : swap_fail_kind == 'w');
        }
        if (inject || disk_off + 512 > disk.size()) {
            if (inject) swap_errors++;
            m_ata_active = false; m_ata_writing = false;
            m_ata_status = 0x41; m_ata_error = 0x10;
            ata_interrupt();
            return;
        }
        if (cmd == 0x20) {
            if (m_ata_dh & 0x10) swap_reads++;
            // READ SECTORS: load sector into buffer, set DRQ
            memset(m_ata_buf, 0, 512);
            if (disk_off < disk.size()) {
                size_t avail = disk.size() - disk_off;
                if (avail > 512) avail = 512;
                memcpy(m_ata_buf, disk.data() + disk_off, avail);
            }
            m_ata_buf_idx = 0;
            m_ata_active = true;
            m_ata_writing = false;
            m_ata_status = 0x48;  // DRDY + DRQ
            m_ata_error = 0;
            ata_interrupt();
        } else if (cmd == 0x30) {
            // WRITE SECTORS: set DRQ, driver fills buffer
            memset(m_ata_buf, 0, 512);
            m_ata_buf_idx = 0;
            m_ata_active = true;
            m_ata_writing = true;
            m_ata_status = 0x48;  // DRDY + DRQ
            m_ata_error = 0;
        } else {
            m_ata_status = 0x41;  // DRDY + ERR
            m_ata_error = 0x04;   // abort
        }
    }

    void ata_flush_write() {
        auto &disk = (m_ata_dh & 0x10) ? m_swap : m_hd;
        unsigned lba = m_ata_sn | ((unsigned)m_ata_cl << 8);
        unsigned disk_off = lba * 512;
        if (disk_off + 512 > disk.size()) {
            disk.resize(disk_off + 512, 0);
        }
        if (m_ata_dh & 0x10) swap_writes++;
        memcpy(disk.data() + disk_off, m_ata_buf, 512);
        m_ata_active = false;
        m_ata_status = 0x40;  // DRDY, clear DRQ
        m_ata_error = 0;
        ata_interrupt();
    }

    void ata_interrupt() {
        if ((m_ata_dh & 0x10) && swap_delay)
            m_swap_irq_due = m_cpu->get_cycles() + swap_delay;
        else m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
    }
    uint64_t m_swap_irq_due = 0;
    unsigned m_swap_read_cmds = 0, m_swap_write_cmds = 0;

    bool m_trace;
    MemoryRegion *m_memory;
    MMU *m_mmu;
    z8001_device *m_cpu;
    std::vector<uint8_t> m_disk;
    std::vector<uint8_t> m_hd, m_swap;
    std::string m_console_buf;
    std::queue<uint8_t> m_console_rx;
    u8 m_dma_blk_hi, m_dma_blk_lo;
    u8 m_dma_addr_hi, m_dma_addr_lo;
    u8 m_dma_status;
    // ATA PIO state
    u8 m_ata_sc, m_ata_sn, m_ata_cl, m_ata_ch, m_ata_dh;
    u8 m_ata_status, m_ata_error;
    u8 m_ata_buf[512];
    unsigned m_ata_buf_idx;
    bool m_ata_active, m_ata_writing;
};

static bool load_file(MemoryRegion& mem, const char* filename, uint32_t addr) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s'\n", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* buf = new uint8_t[size];
    size_t nread = fread(buf, 1, size, f);
    fclose(f);

    if ((long)nread != size) {
        fprintf(stderr, "Error: Short read on '%s'\n", filename);
        delete[] buf;
        return false;
    }

    if (!mem.load(addr, buf, size)) {
        fprintf(stderr, "Error: Failed to load '%s' at 0x%06X\n", filename, addr);
        delete[] buf;
        return false;
    }

    printf("  Loaded %-20s at 0x%06X (%ld bytes)\n", filename, addr, size);
    delete[] buf;
    return true;
}

static std::string decode_input(const char *arg) {
    std::string text;
    for (const char *q = arg; *q; q++) {
        if (q[0] == '\\' && q[1] == 'n') { text += '\n'; q++; }
        else text += *q;
    }
    return text;
}

int main(int argc, char* argv[]) {
    bool trace = false;
    bool reg_trace = false;
    bool mem_trace = false;
    // The emulator counts cycles in 64 bits, so a run is not limited to 2^31.
    uint64_t max_cycles = 50000000;  // increased for shell startup overhead
    unsigned cycles_per_tick = 5000; // accelerated regression default

    // Defaults are the boot test: the small root image, the pipeline typed
    // at the shell, and an exact transcript. -d, -i and -x run something
    // else under the same kernel, e.g. the C library test.
    const char *disk_image = "hd.img";
    const char *boot_rom = nullptr;
    const char *save_image = nullptr, *profile_file = nullptr;
    const char *console_input = "echo hello | cat\nexit\n";
    const char *expect = nullptr;
    const char *wait_output = nullptr;
    std::string typed, later_input;
    bool has_later_input = false;
    uint64_t measure_ticks = 0;
    const char *measure_marker = "# ";

    char fault_kind = 0;
    unsigned fault_offset = 0;
    unsigned ram_kib = 8192, swap_kib = 4096;
    uint64_t swap_delay = 0;
    char swap_fail_kind = 0;
    unsigned swap_fail_nth = 0;
    int opt;
    while ((opt = getopt(argc, argv, "trmc:d:i:x:w:I:n:M:o:P:F:R:S:D:E:b:T:")) != -1) {
        switch (opt) {
            case 'T': {
                char *end;
                unsigned long value = strtoul(optarg, &end, 10);
                if (*end || !*optarg || value < 100 || value > 1000000) {
                    fprintf(stderr, "-T requires clock period from 100 to 1000000 CPU cycles\n"); return 1;
                }
                cycles_per_tick = value; break;
            }
            case 'D': {
                char *end;
                swap_delay = strtoull(optarg, &end, 10);
                if (*end || !*optarg || swap_delay > 10000000) {
                    fprintf(stderr, "-D requires swap IRQ delay cycles from 0 to 10000000\n"); return 1;
                }
                break;
            }
            case 'E': {
                char extra;
                if (sscanf(optarg, "%c:%u%c", &swap_fail_kind, &swap_fail_nth, &extra) != 2 ||
                    (swap_fail_kind != 'r' && swap_fail_kind != 'w') || !swap_fail_nth) {
                    fprintf(stderr, "-E requires r:N or w:N for one failing swap command\n"); return 1;
                }
                break;
            }
            case 'S': {
                char *end;
                unsigned long value = strtoul(optarg, &end, 10);
                if (*end || !*optarg || value > 16000) {
                    fprintf(stderr, "-S requires swap KiB from 0 to 16000\n"); return 1;
                }
                swap_kib = value; break;
            }
            case 'R': {
                char *end;
                unsigned long value = strtoul(optarg, &end, 10);
                if (*end || !*optarg || value < 128 || value > 8192 || value % 2) {
                    fprintf(stderr, "-R requires even RAM KiB from 128 to 8192\n");
                    return 1;
                }
                ram_kib = value;
                break;
            }
            case 'F': {
                char extra;
                if (sscanf(optarg, "%c:%x%c", &fault_kind, &fault_offset, &extra) != 2 ||
                    (fault_kind != 'r' && fault_kind != 'w' && fault_kind != 'u') ||
                    fault_offset > 0xffff) {
                    fprintf(stderr, "-F requires r:hex, w:hex or u:hex\n"); return 1;
                }
                break;
            }
            case 't': trace = true; break;
            case 'b': boot_rom = optarg; break;
            case 'r': reg_trace = true; break;
            case 'm': mem_trace = true; break;
            case 'c': max_cycles = strtoull(optarg, nullptr, 10); break;
            case 'd': disk_image = optarg; break;
            case 'o': save_image = optarg; break;
            case 'P': profile_file = optarg; break;
            case 'i': {
                // "\n" written as two characters stands for a newline, so the
                // text survives make and the shell unchanged.
                typed = decode_input(optarg);
                console_input = typed.c_str();
                break;
            }
            case 'x': expect = optarg; break;
            case 'w': wait_output = optarg; break;
            case 'I': later_input = decode_input(optarg); has_later_input = true; break;
            case 'n': measure_ticks = strtoull(optarg, nullptr, 10); break;
            case 'M': measure_marker = optarg; break;
            default:
                fprintf(stderr, "Usage: %s [-t] [-r] [-m] [-c cycles] "
                        "[-d hd-image] [-b boot-ROM] [-T cycles-per-tick] [-i console-input] [-x expected-text] "
                        "[-w output-marker -I later-input] "
                        "[-n measured-ticks -M start-marker] "
                        "[-o saved-hd-image] [-P user-memory.tsv] [-F r|w|u:hex] [-R ram-KiB] [-S swap-KiB] [-D swap-IRQ-cycles] [-E r|w:N]\n", argv[0]);
                return 1;
        }
    }
    if ((wait_output != nullptr) != has_later_input) {
        fprintf(stderr, "-w and -I must be supplied together\n");
        return 1;
    }

    if (fault_kind && !wait_output) {
        fprintf(stderr, "-F requires -w/-I to arm after guest setup\n"); return 1;
    }

    printf("Z8001 Kernel Test Driver\n");
    printf("========================\n");

    // Backing store for the bus address space; MMU rejects absent low RAM.
    MemoryRegion memory(0x800000);
    memory.set_name("MEM");
    memory.set_trace(mem_trace);

    // Create paged MMU wrapping physical memory
    MMU mmu(&memory, ram_kib / 2);
    printf("Installed low RAM: %u KiB; EPU bank reserved separately\n", ram_kib);

    // Create Z8001 CPU (memory access goes through MMU)
    ClockObservedCPU cpu;


    // Create I/O ports with DMA controller, MMU access, and CPU reference
    KernelIOPorts io(&memory, &mmu, &cpu);

    printf("Loading binaries:\n");

    if (ram_kib < (boot_rom ? 256U : 192U)) {
        fprintf(stderr, "insufficient memory: direct boot requires 192 KiB; disk bootstrap requires 256 KiB\n");
        return 1;
    }
    if (boot_rom) {
        // The same small ROM used by MAME boots the disk; no kernel is preloaded.
        if (!load_file(memory, boot_rom, 0)) return 1;
    } else {
    // Load ROM at segment 0 (physical address 0x000000)
    if (!load_file(memory, "rom.bin", 0x000000))
        return 1;

    // Kernel instruction storage is physical bank 2. PSA vectors use ROM data.
    if (!load_file(memory, "kernel.bin", 0x020000))
        return 1;

    if (!load_file(memory, "kernel.bin", 0x001000)) return 1;
    if (!load_file(memory, "handler-data.bin", 0x010000)) return 1;
    // Kernel text starts at logical 1:0200, backed by physical bank 2.
    if (!load_file(memory, "handler.bin", 0x020200))
        return 1;
    if (!load_file(memory, "fpe.bin", 0x7f0000))
        return 1;
    }

    // Load disk images
    if (!io.load_disk("root.img"))
        return 1;
    io.swap_size(swap_kib);
    io.swap_delay = swap_delay;
    io.swap_fail_kind = swap_fail_kind;
    io.swap_fail_nth = swap_fail_nth;
    if (!io.load_hd(disk_image))
        return 1;

    InstructionBus instructions(mmu);
    cpu.set_memory(&mmu);
    cpu.set_program_memory(&instructions);
    FirstWordBus first_words(mmu);

    cpu.set_opcode_bus(&first_words);
    cpu.set_io(&io);
    cpu.set_trace(trace);
    cpu.set_reg_trace(reg_trace);
    io.set_trace(trace);

    // Reset CPU - reads reset vector from address 0x000000
    cpu.reset();
    cpu.step(); // Reset vector cycles precede normal/system protection.
    mmu.set_fault(&cpu, fault_kind, fault_offset);
    FILE *profile_output = profile_file ? fopen(profile_file, "w") : nullptr;
    if (profile_file && !profile_output) { perror(profile_file); return 1; }
    UserProfile profile(cpu, mmu, profile_output);
    if (profile_output) {
        instructions.profile = &profile;
        first_words.profile = &profile;
        cpu.set_trap_callback(UserProfile::trap, &profile);
    }

    printf("\nReset state:\n");
    printf("  FCW: 0x%04X\n", cpu.get_fcw());
    printf("  PC:  0x%06X\n", cpu.get_pc());
    printf("\nRunning (max %llu cycles)...\n", static_cast<unsigned long long>(max_cycles));
    if (trace) printf("---\n");

    // Run CPU in chunks, delivering periodic NVI clock ticks
    // and delayed console input for testing read()
    const unsigned CYCLES_PER_TICK = cycles_per_tick;
    uint64_t tick_count = 0, merged_ticks = 0;
    uint64_t start_ticks = 0, start_merged = 0, start_accepted = 0;
    bool measuring = false, measure_done = false, start_pending = false;
    int input_idx = 0;

    bool input_started = false;
    bool boot_input_sent = false;
    int idle_after_input = 0;
    bool waiting_for_output = has_later_input;
    int ticks_after_marker = 0;

    while (cpu.get_cycles() < max_cycles) {
        cpu.run(CYCLES_PER_TICK);
        io.poll_swap();
        if (fault_kind && io.console_output().find(wait_output) != std::string::npos)
            mmu.arm_fault();
        // Sample before injection: the previous pulse has had a full slice
        // to be accepted. A still-set latch at injection proves a merged tick.
        if (measure_ticks && !measuring &&
            io.console_output().find(measure_marker) != std::string::npos) {
            measuring = true;
            start_ticks = tick_count;
            start_merged = merged_ticks;
            start_accepted = cpu.clock_accepted;
            start_pending = cpu.clock_pending();
            printf("\nClock baseline: generated=%llu merged=%llu pending=%d\n",
                   (unsigned long long)tick_count,
                   (unsigned long long)merged_ticks, start_pending);
        }
        if (measuring && tick_count - start_ticks == measure_ticks) {
            uint64_t lost = merged_ticks - start_merged;
            bool pending = cpu.clock_pending();
            uint64_t accepted = cpu.clock_accepted - start_accepted;
            if (accepted != measure_ticks - lost + start_pending - pending) {
                fprintf(stderr, "FAIL: clock accounting mismatch\n");
                return 1;
            }
            printf("\nClock sample: generated=%llu accepted=%llu merged=%llu "
                   "pending_start=%d pending_end=%d loss_pct=%.9f\n",
                   (unsigned long long)measure_ticks,
                   (unsigned long long)accepted, (unsigned long long)lost,
                   start_pending, pending, 100.0 * lost / measure_ticks);
            measure_done = true;
            break;
        }
        // Always deliver NVI clock tick (wakes CPU from HALT)
        if (cpu.clock_pending()) merged_ticks++;
        cpu.pulse_input_line(z8002_device::NVI_LINE);
        tick_count++;
        // Select /unix at the standalone loader prompt in ROM-boot tests.
        if (boot_rom && !boot_input_sent && io.console_output().find(": ") != std::string::npos) {
            io.queue_console_char('\n');
            boot_input_sent = true;
        }
        // Deliver console input after shell prompt "# " appears
        if (console_input[input_idx]) {
            if (!input_started && io.console_output().find("# ") != std::string::npos) {
                input_started = true;
            }
            if (input_started) {
                io.queue_console_char(console_input[input_idx++]);
            }
        }
        if (!console_input[input_idx] && waiting_for_output &&
            io.console_output().find(wait_output) != std::string::npos) {
            // Give a CPU-bound child time to run after the marker's write.
            if (++ticks_after_marker >= 100) {
                console_input = later_input.c_str();
                input_idx = 0;
                waiting_for_output = false;
                idle_after_input = 0;
            }
        }
        // After all input delivered, count idle ticks
        if (!console_input[input_idx] && !waiting_for_output) {
            idle_after_input++;
        }
        // Stop if halted AND all input delivered AND enough time for pipe to finish
        if (!measure_ticks && cpu.is_halted() && !console_input[input_idx] &&
            !waiting_for_output && idle_after_input > 500 &&
            (!expect || io.console_output().find(expect) != std::string::npos)) {
            break;
        }
    }

    if (profile_output) { profile.flush(); fclose(profile_output); }
    if (save_image && !io.save_hd(save_image)) { perror(save_image); return 1; }
    if (trace) printf("---\n");

    // Dump final state
    printf("\nFinal state:\n");
    cpu.dump_regs();
    printf("\nTotal cycles: %llu\n", static_cast<unsigned long long>(cpu.get_cycles()));
    printf("Clock ticks generated: %llu\n", (unsigned long long)tick_count);
    printf("Clock ticks merged: %llu; pending: %d\n",
           (unsigned long long)merged_ticks, cpu.clock_pending());
    printf("Clock interrupts accepted: %llu\n",
           (unsigned long long)cpu.clock_accepted);
    if (tick_count != cpu.clock_accepted + merged_ticks + cpu.clock_pending()) {
        fprintf(stderr, "FAIL: total clock accounting mismatch\n");
        return 1;
    }
    printf("Halted: %s\n", cpu.is_halted() ? "Yes" : "No");

    // Dump system stack (IRET frame from trap handler)
    if (cpu.is_halted()) {
        uint16_t sp = cpu.get_reg(15);  // R15 = stack offset
        printf("\nSystem stack at seg1:%04X:\n", sp);
        for (int i = 0; i < 16; i++) {
            uint32_t addr = 0x010000 + sp + i * 2;  // segment 1
            uint16_t w = memory.read_word(addr);
            printf("  [%04X] = %04X\n", sp + i * 2, w);
        }
    }

    // Verify: console output contains kernel msg, shell prompt, echo output, no panics
    if (swap_delay || swap_fail_nth)
        printf("Swap-wait user samples: %u; injected swap errors: %u\n",
               io.swap_user_samples, io.swap_errors);
    std::string output = io.console_output();
    bool has_kernel_msg = output.find("Z8000 Unix") != std::string::npos;
    bool has_prompt = output.find("# ") != std::string::npos;
    // The command line itself is echoed and contains "hello", so only a line
    // that ends right after it ("hello\n", with or without "\r") is the
    // pipeline's own output. The shell must then prompt again, and the system
    // must come to rest in idle() rather than run into the cycle limit.
    size_t hello_at = output.find("hello\r\n");
    if (hello_at == std::string::npos) hello_at = output.find("hello\n");
    bool has_hello = hello_at != std::string::npos;
    bool has_prompt_after = has_hello && output.find("# ", hello_at) != std::string::npos;
    bool settled = cpu.is_halted();
    bool has_panic = output.find("panic") != std::string::npos;
    if (measure_ticks) {
        bool ok = measure_done && has_kernel_msg && has_prompt && !has_panic &&
                  (!expect || output.find(expect) != std::string::npos) &&
                  output.find("FAIL") == std::string::npos;
        printf("%s: clock measurement %s\n", ok ? "PASS" : "FAIL",
               measure_done ? "complete" : "incomplete (cycle limit or marker)");
        return ok ? 0 : 1;
    }

    printf("Absent RAM accesses: %u\n", mmu.absent_count);
    printf("Unmapped accesses: %u\n", mmu.unmapped_count);
    printf("Shared text peak mappings: %u\n", mmu.shared_text_peak);
    printf("Swap sectors: %u read, %u written\n", io.swap_reads, io.swap_writes);
    printf("Protection faults: %u; stack warnings: %u\n", mmu.protection_count, mmu.warning_count);
    if (fault_kind) printf("MMU denied accesses: %u\n", mmu.fault_count);
    printf("\nConsole output: \"");
    for (char c : output) {
        if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c < 0x20) printf("\\x%02x", (unsigned char)c);
        else putchar(c);
    }
    printf("\"\n\n");

    if (expect) {
        // A custom run: the given text must appear, the system must settle,
        // and there must be no panic.
        bool found = output.find(expect) != std::string::npos;
        if (found && settled && !has_panic) {
            printf("PASS: output contains the expected text\n");
            return 0;
        }
        printf("FAIL: expected_text=%s, panic=%s, halted=%s\n",
               found ? "yes" : "no", has_panic ? "yes" : "no", settled ? "yes" : "no");
        return 1;
    }

    // With the checks above passed, the whole transcript must also match: the
    // typed characters are echoed exactly, "exit" is typed ahead while the
    // pipeline runs, and the shell prompts once more before it exits.
    const std::string expected =
        "boot\nZ8000 Unix\n# echo hello | cat\r\nexit\r\nhello\r\n# # ";
    bool exact = output == expected;
    if (!exact)
        printf("Console output differs from the expected transcript.\n");

    if (has_kernel_msg && has_prompt && has_hello && has_prompt_after && settled && !has_panic && exact) {
        printf("PASS: Kernel booted, shell ran, echo hello | cat printed hello\n");
        return 0;
    } else {
        printf("FAIL: exact=%s, kernel_msg=%s, prompt=%s, pipeline_output=%s, prompt_after=%s, panic=%s, halted=%s\n",
               exact ? "yes" : "no",
               has_kernel_msg ? "yes" : "no",
               has_prompt ? "yes" : "no",
               has_hello ? "yes" : "no",
               has_prompt_after ? "yes" : "no",
               has_panic ? "yes" : "no",
               settled ? "yes" : "no");
        return 1;
    }
}
