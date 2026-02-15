// Test Driver for Z8001 Kernel Bring-up
//
// Loads ROM (segment 0) and kernel (segment 1) into an 8MB Z8001 address
// space, runs the CPU, and verifies that the SYSCALL trap handler executed
// successfully by checking R0 == 42 (exit status).
//
// Usage: ./test_driver [-t] [-r] [-m]
//   -t  Enable instruction tracing
//   -r  Enable register tracing
//   -m  Enable memory tracing

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <getopt.h>
#include "z8000.h"
#include "memory.h"

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
    int max_cycles = 10000;  // safety limit

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

    // Create I/O ports (not used in this test, but required by CPU)
    IOPorts io;

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

    // Create Z8001 CPU
    z8001_device cpu;
    cpu.set_memory(&memory);
    cpu.set_io(&io);
    cpu.set_trace(trace);
    cpu.set_reg_trace(reg_trace);

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

    // Verify: R0 = 42 (exit status), console output = test message
    uint16_t r0 = cpu.get_reg(0);
    std::string expected_output = "Hello from Z8000 Unix!\n";
    printf("\n");
    if (r0 == 42 && cpu.is_halted() && io.console_output() == expected_output) {
        printf("PASS: R0 = %d (exit status), console output correct\n", r0);
        return 0;
    } else {
        printf("FAIL: R0 = %d (expected 42), halted = %s, console = \"%s\"\n",
               r0, cpu.is_halted() ? "yes" : "no", io.console_output().c_str());
        return 1;
    }
}
