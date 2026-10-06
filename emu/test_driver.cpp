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

// Observe the existing request latch without changing interrupt delivery.
class ClockObservedCPU : public z8001_device {
public:
    bool clock_pending() const { return (m_irq_req & Z8000_NVI) != 0; }
    uint64_t clock_accepted = 0;
protected:
    uint16_t GET_FCW(uint32_t vec) override {
        // Count actual NVI dispatches, independently of the latch accounting.
        if (vec == PSA_ADDR() + m_vector_mult * 0x18) clock_accepted++;
        return z8001_device::GET_FCW(vec);
    }
};

// Paged MMU: 128 segments x 32 pages x 2KB pages
// Identity-mapped on construction; UPAGE/WPAGE ports remap specific pages.
class MMU : public z8000_memory_bus {
public:
    MMU(MemoryRegion *phys) : m_phys(phys), m_trace(false) {
        // Identity map: frame = seg * 32 + page
        for (int seg = 0; seg < 128; seg++) {
            m_iseg[seg] = seg;
            for (int page = 0; page < 32; page++)
                m_pages[seg][page] = seg * 32 + page;
        }
        set_upage(62);
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
        return m_phys->read_byte(translate(addr));
    }

    u16 read_word(u32 addr) override {
        return m_phys->read_word(translate(addr));
    }

    void write_byte(u32 addr, u8 val) override {
        m_phys->write_byte(translate(addr), val);
    }

    void write_word(u32 addr, u16 val) override {
        m_phys->write_word(translate(addr), val);
    }

    void write_word(u32 addr, u16 val, u16 mask) override {
        m_phys->write_word(translate(addr), val, mask);
    }

private:
    MemoryRegion *m_phys;
    uint16_t m_pages[128][32];
    uint8_t m_iseg[128];
    bool m_trace;
};

// All instruction-space accesses, including operands and PC-relative loads.
class InstructionBus : public z8000_memory_bus {
public:
    explicit InstructionBus(MMU &mmu) : m_mmu(mmu) {}
    u8 read_byte(u32 a) override { return m_mmu.read_byte(m_mmu.instruction_address(a)); }
    u16 read_word(u32 a) override { return m_mmu.read_word(m_mmu.instruction_address(a)); }
    void write_byte(u32 a, u8 v) override { m_mmu.write_byte(m_mmu.instruction_address(a), v); }
    void write_word(u32 a, u16 v) override { m_mmu.write_word(m_mmu.instruction_address(a), v); }
    void write_word(u32 a, u16 v, u16 mask) override {
        m_mmu.write_word(m_mmu.instruction_address(a), v, mask);
    }
private:
    MMU &m_mmu;
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
        unsigned lba = m_ata_sn | ((unsigned)m_ata_cl << 8);
        unsigned disk_off = lba * 512;

        if (cmd == 0x20) {
            // READ SECTORS: load sector into buffer, set DRQ
            memset(m_ata_buf, 0, 512);
            if (disk_off < m_hd.size()) {
                size_t avail = m_hd.size() - disk_off;
                if (avail > 512) avail = 512;
                memcpy(m_ata_buf, m_hd.data() + disk_off, avail);
            }
            m_ata_buf_idx = 0;
            m_ata_active = true;
            m_ata_writing = false;
            m_ata_status = 0x48;  // DRDY + DRQ
            m_ata_error = 0;
            m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
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
        unsigned lba = m_ata_sn | ((unsigned)m_ata_cl << 8);
        unsigned disk_off = lba * 512;
        if (disk_off + 512 > m_hd.size()) {
            m_hd.resize(disk_off + 512, 0);
        }
        memcpy(m_hd.data() + disk_off, m_ata_buf, 512);
        m_ata_active = false;
        m_ata_status = 0x40;  // DRDY, clear DRQ
        m_ata_error = 0;
        m_cpu->pulse_input_line(z8002_device::VI_LINE, 0);
    }

    bool m_trace;
    MemoryRegion *m_memory;
    MMU *m_mmu;
    z8001_device *m_cpu;
    std::vector<uint8_t> m_disk;
    std::vector<uint8_t> m_hd;
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

    // Defaults are the boot test: the small root image, the pipeline typed
    // at the shell, and an exact transcript. -d, -i and -x run something
    // else under the same kernel, e.g. the C library test.
    const char *disk_image = "hd.img";
    const char *console_input = "echo hello | cat\nexit\n";
    const char *expect = nullptr;
    const char *wait_output = nullptr;
    std::string typed, later_input;
    bool has_later_input = false;
    uint64_t measure_ticks = 0;
    const char *measure_marker = "# ";

    int opt;
    while ((opt = getopt(argc, argv, "trmc:d:i:x:w:I:n:M:")) != -1) {
        switch (opt) {
            case 't': trace = true; break;
            case 'r': reg_trace = true; break;
            case 'm': mem_trace = true; break;
            case 'c': max_cycles = strtoull(optarg, nullptr, 10); break;
            case 'd': disk_image = optarg; break;
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
                        "[-d hd-image] [-i console-input] [-x expected-text] "
                        "[-w output-marker -I later-input] "
                        "[-n measured-ticks -M start-marker]\n", argv[0]);
                return 1;
        }
    }
    if ((wait_output != nullptr) != has_later_input) {
        fprintf(stderr, "-w and -I must be supplied together\n");
        return 1;
    }

    printf("Z8001 Kernel Test Driver\n");
    printf("========================\n");

    // Create 8MB memory region (Z8001 segmented address space)
    MemoryRegion memory(0x800000);
    memory.set_name("MEM");
    memory.set_trace(mem_trace);

    // Create paged MMU wrapping physical memory
    MMU mmu(&memory);

    // Create Z8001 CPU (memory access goes through MMU)
    ClockObservedCPU cpu;

    // Create I/O ports with DMA controller, MMU access, and CPU reference
    KernelIOPorts io(&memory, &mmu, &cpu);

    printf("Loading binaries:\n");

    // Load ROM at segment 0 (physical address 0x000000)
    if (!load_file(memory, "rom.bin", 0x000000))
        return 1;

    // Load kernel at segment 1 (physical address 0x010000)
    if (!load_file(memory, "kernel.bin", 0x010000))
        return 1;

    // Load C handler at segment 1, offset 0x0200
    if (!load_file(memory, "handler.bin", 0x010200))
        return 1;
    if (!load_file(memory, "fpe.bin", 0x7f0000))
        return 1;

    // Load disk images
    if (!io.load_disk("root.img"))
        return 1;
    if (!io.load_hd(disk_image))
        return 1;

    InstructionBus instructions(mmu);
    cpu.set_memory(&mmu);
    cpu.set_program_memory(&instructions);
    cpu.set_io(&io);
    cpu.set_trace(trace);
    cpu.set_reg_trace(reg_trace);
    io.set_trace(trace);

    // Reset CPU - reads reset vector from address 0x000000
    cpu.reset();

    printf("\nReset state:\n");
    printf("  FCW: 0x%04X\n", cpu.get_fcw());
    printf("  PC:  0x%06X\n", cpu.get_pc());
    printf("\nRunning (max %llu cycles)...\n", static_cast<unsigned long long>(max_cycles));
    if (trace) printf("---\n");

    // Run CPU in chunks, delivering periodic NVI clock ticks
    // and delayed console input for testing read()
    const int CYCLES_PER_TICK = 5000;
    uint64_t tick_count = 0, merged_ticks = 0;
    uint64_t start_ticks = 0, start_merged = 0, start_accepted = 0;
    bool measuring = false, measure_done = false, start_pending = false;
    int input_idx = 0;

    bool input_started = false;
    int idle_after_input = 0;
    bool waiting_for_output = has_later_input;
    int ticks_after_marker = 0;

    while (cpu.get_cycles() < max_cycles) {
        cpu.run(CYCLES_PER_TICK);
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
            !waiting_for_output && idle_after_input > 500) {
            break;
        }
    }

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
