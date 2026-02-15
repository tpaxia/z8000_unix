// Test Driver for Z8001 Kernel Bring-up
//
// Loads ROM (segment 0), kernel (segment 1), and C handler into an 8MB
// Z8001 address space, runs the CPU, and verifies that the kernel boots
// to process 0, mounts the filesystem, opens /dev/console, and prints
// "Z8000 Unix\n".
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
#include <getopt.h>
#include "z8000.h"
#include "memory.h"

// Extended IOPorts with DMA controller for RAM disk
class KernelIOPorts : public z8000_io_bus {
public:
    KernelIOPorts(MemoryRegion *mem)
        : m_trace(false), m_memory(mem),
          m_dma_blk_hi(0), m_dma_blk_lo(0),
          m_dma_addr_hi(0), m_dma_addr_lo(0),
          m_dma_status(0)
    {
    }

    const std::string& console_output() const { return m_console_buf; }
    void set_trace(bool enable) { m_trace = enable; }

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

    // z8000_io_bus interface
    u8 read_byte(u16 addr, int mode) override {
        u8 val;
        if (mode == 0) {
            switch (addr) {
                case 0x00E5:  // DMA status
                    val = m_dma_status;
                    break;
                case 0x00F0:
                    val = 0x00;  // Console RX data (no input)
                    break;
                case 0x00F2:
                    val = 0x01;  // Console status: TX ready
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
        }
    }

    void write_word(u16 addr, u16 val, int mode) override {
        addr &= 0xFFFE;
        if (m_trace) {
            printf("  %sI/O WR16 [%04X] <- %04X\n", mode ? "S" : "", addr, val);
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

    bool m_trace;
    MemoryRegion *m_memory;
    std::vector<uint8_t> m_disk;
    std::string m_console_buf;
    u8 m_dma_blk_hi, m_dma_blk_lo;
    u8 m_dma_addr_hi, m_dma_addr_lo;
    u8 m_dma_status;
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
    int max_cycles = 500000;  // increased for V7 init

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

    // Create I/O ports with DMA controller
    KernelIOPorts io(&memory);

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

    // Load disk image
    if (!io.load_disk("root.img"))
        return 1;

    // Create Z8001 CPU
    z8001_device cpu;
    cpu.set_memory(&memory);
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

    // Run CPU
    cpu.run(max_cycles);

    if (trace) printf("---\n");

    // Dump final state
    printf("\nFinal state:\n");
    cpu.dump_regs();
    printf("\nTotal cycles: %d\n", cpu.get_cycles());
    printf("Halted: %s\n", cpu.is_halted() ? "Yes" : "No");

    // Verify: CPU halted, console output contains "Z8000 Unix"
    std::string output = io.console_output();
    bool has_message = output.find("Z8000 Unix") != std::string::npos;
    bool has_panic = output.find("panic") != std::string::npos;

    printf("\nConsole output: \"");
    for (char c : output) {
        if (c == '\n') printf("\\n");
        else putchar(c);
    }
    printf("\"\n\n");

    if (cpu.is_halted() && has_message && !has_panic) {
        printf("PASS: Kernel booted, mounted root, opened console, printed message\n");
        return 0;
    } else {
        printf("FAIL: halted=%s, message=%s, panic=%s\n",
               cpu.is_halted() ? "yes" : "no",
               has_message ? "yes" : "no",
               has_panic ? "yes" : "no");
        return 1;
    }
}
