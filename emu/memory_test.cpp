// Test the actual board MMU bus without executing guest instructions.
#define main kernel_test_main
#include "test_driver.cpp"
#undef main
#include <stdexcept>

int main() {
    MemoryRegion physical(0x800000);
    ClockObservedCPU cpu;
    MMU mmu(&physical, 1063); // A non-power-of-two installed RAM size.
    // Boundary checks run as system accesses; protection is tested separately.
    auto require = [](bool ok) {
        if (!ok) throw std::runtime_error("physical RAM boundary regression");
    };
    unsigned end = 1063 * 2048;
    mmu.select_page(64); mmu.set_page(1062);
    mmu.select_page(65); mmu.set_page(1063);
    mmu.write_word(0x0207fe, 0x1234);
    require(mmu.read_word(0x0207fe) == 0x1234);
    physical.write_word(end, 0x5678);
    require(mmu.read_word(0x020800) == 0);
    mmu.write_word(0x020800, 0xabcd);
    require(physical.read_word(end) == 0x5678);
    mmu.set_wpage(1063);
    require(mmu.read_byte(0x01e000) == 0);
    mmu.write_byte(0x01e000, 0xaa);
    require(physical.read_word(end) == 0x5678);
    require(mmu.absent_count == 4);
    mmu.write_word(0x7f0000, 0x4321);
    require(mmu.read_word(0x7f0000) == 0x4321);
    mmu.set_upage(1060);
    mmu.write_word(0x01f000, 0x1122);
    require(mmu.read_word(0x7ff000) == 0x1122);
    require(mmu.absent_count == 4);
    mmu.set_wpage(4096); // Invalid frame must not wrap into the backing store.
    require(mmu.read_word(0x01e000) == 0);
    mmu.write_word(0x01e000, 0xffff);
    require(mmu.absent_count == 6);
    // Unallocated banks fault; indexed maps can be reassigned and invalidated.
    require(mmu.read_word(0x030000) == 0);
    require(mmu.absent_count == 6 && mmu.unmapped_count == 1);
    mmu.select_page(96); mmu.set_page(100);
    mmu.write_word(0x030000, 0x9876);
    require(physical.read_word(100 * 2048) == 0x9876);
    mmu.select_page(96); mmu.set_page(101);
    require(mmu.read_word(0x030000) == 0);
    mmu.select_page(96); mmu.set_page(0xffff);
    mmu.write_word(0x030000, 0xaaaa);
    require(mmu.absent_count == 6 && mmu.unmapped_count == 2);
    require(physical.read_word(100 * 2048) == 0x9876);
    cpu.init_state(0x4000, 0x010200, 0, 0, 0, 0);
    mmu.set_fault(&cpu, 0, 0);
    mmu.acknowledge();
    mmu.select_page(64); mmu.set_page(100 | MM_RO);
    mmu.first_word(0x020010);
    mmu.write_word(0x020000, 0xabcd);
    require(physical.read_word(100*2048) == 0x9876);
    require(mmu.fault_register(MM_FAULT) == (MF_VALID|MF_WRITE|MF_PROT));
    require(mmu.fault_register(MM_PC) == 0x10 && mmu.fault_register(MM_PCSEG) == 2);
    mmu.acknowledge();
    cpu.init_state(0, 0x020100, 0, 0, 0, 0);
    require(mmu.read_word(0x010000) == 0);
    require(mmu.fault_register(MM_FAULT) == (MF_VALID|MF_READ|MF_PROT));
    mmu.acknowledge();
    mmu.select_page(64); mmu.set_page(100);
    mmu.stack_select(2); mmu.stack_base(0);
    mmu.first_word(0x020020);
    mmu.write_word(0x020010, 0x1234);
    require(physical.read_word(100*2048+16) == 0x1234);
    require(mmu.fault_register(MM_FAULT) == (MF_VALID|MF_WRITE|MF_WARN));
    require(mmu.fault_register(MM_FLOW) == 16 && mmu.fault_register(MM_FHIGH) == 17);
    mmu.first_word(0x020022);
    mmu.write_word(0x020012, 0x5678);
    require(mmu.fault_register(MM_FAULT) & MF_MIXED);
    mmu.acknowledge();
    require(mmu.fault_register(MM_FAULT) == 0);
    puts("RAM bounds, mappings, protection and fault latches: passed");
}
