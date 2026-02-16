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
// Usage: ./test_driver [-t] [-r] [-m] [-c cycles]
//   -t  Enable instruction tracing
//   -r  Enable register tracing
//   -m  Enable memory tracing

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <queue>
#include <getopt.h>
#include "z8000.h"
#include "memory.h"

// Paged MMU: 128 segments x 32 pages x 2KB pages
// Identity-mapped on construction; UPAGE/WPAGE ports remap specific pages.
class MMU : public z8000_memory_bus {
public:
    MMU(MemoryRegion *phys) : m_phys(phys), m_trace(false) {
        // Identity map: frame = seg * 32 + page
        for (int seg = 0; seg < 128; seg++)
            for (int page = 0; page < 32; page++)
                m_pages[seg][page] = seg * 32 + page;
    }

    void set_trace(bool enable) { m_trace = enable; }

    // KDSA6 equivalent: remap seg1 pages 30-31 (virtual 0xF000-0xFFFF)
    void set_upage(uint16_t frame) {
        m_pages[1][30] = frame;
        m_pages[1][31] = frame + 1;
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
    bool m_trace;
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
        m_cpu->assert_vi(0);
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
            m_cpu->assert_vi(0);
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
        m_cpu->assert_vi(0);
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

int main(int argc, char* argv[]) {
    bool trace = false;
    bool reg_trace = false;
    bool mem_trace = false;
    int max_cycles = 10000000;  // increased for clock interrupt overhead

    int opt;
    while ((opt = getopt(argc, argv, "trmc:")) != -1) {
        switch (opt) {
            case 't': trace = true; break;
            case 'r': reg_trace = true; break;
            case 'm': mem_trace = true; break;
            case 'c': max_cycles = atoi(optarg); break;
            default:
                fprintf(stderr, "Usage: %s [-t] [-r] [-m] [-c cycles]\n", argv[0]);
                return 1;
        }
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
    z8001_device cpu;

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

    // Load disk images
    if (!io.load_disk("root.img"))
        return 1;
    if (!io.load_hd("hd.img"))
        return 1;

    cpu.set_memory(&mmu);
    cpu.set_io(&io);
    cpu.set_trace(trace);
    cpu.set_reg_trace(reg_trace);
    io.set_trace(trace);

    // Reset CPU - reads reset vector from address 0x000000
    cpu.reset();

    printf("\nReset state:\n");
    printf("  FCW: 0x%04X\n", cpu.get_fcw());
    printf("  PC:  0x%06X\n", cpu.get_pc());
    printf("\nRunning (max %d cycles)...\n", max_cycles);
    if (trace) printf("---\n");

    // Run CPU in chunks, delivering periodic NVI clock ticks
    // and delayed console input for testing read()
    const int CYCLES_PER_TICK = 5000;
    int tick_count = 0;
    const char *console_input = "hi\n";
    int input_idx = 0;

    while (!cpu.is_halted() && cpu.get_cycles() < max_cycles) {
        cpu.run(CYCLES_PER_TICK);
        if (!cpu.is_halted()) {
            cpu.assert_nvi();  // clock tick
            tick_count++;
            // Deliver console input after kernel is ready (100 ticks)
            if (tick_count >= 100 && console_input[input_idx]) {
                io.queue_console_char(console_input[input_idx++]);
            }
        }
    }

    if (trace) printf("---\n");

    // Dump final state
    printf("\nFinal state:\n");
    cpu.dump_regs();
    printf("\nTotal cycles: %d\n", cpu.get_cycles());
    printf("Halted: %s\n", cpu.is_halted() ? "Yes" : "No");

    // Verify: CPU halted, console output contains kernel msg + echo, no panics
    std::string output = io.console_output();
    bool has_kernel_msg = output.find("Z8000 Unix") != std::string::npos;
    bool has_echo = output.find("hi") != std::string::npos;
    bool has_panic = output.find("panic") != std::string::npos;

    printf("\nConsole output: \"");
    for (char c : output) {
        if (c == '\n') printf("\\n");
        else putchar(c);
    }
    printf("\"\n\n");

    if (cpu.is_halted() && has_kernel_msg && has_echo && !has_panic) {
        printf("PASS: Kernel booted, console read+write with TTY subsystem succeeded\n");
        return 0;
    } else {
        printf("FAIL: halted=%s, kernel_msg=%s, echo=%s, panic=%s\n",
               cpu.is_halted() ? "yes" : "no",
               has_kernel_msg ? "yes" : "no",
               has_echo ? "yes" : "no",
               has_panic ? "yes" : "no");
        return 1;
    }
}
