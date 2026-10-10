// Included after MMU and UserProfileMemory in test_driver.cpp.
class Z8002ProfileMemory : public UserProfileMemory {
    MMU &mmu;
public:
    explicit Z8002ProfileMemory(MMU &m) : mmu(m) {}
    uint32_t code_address(uint32_t address) const override { return mmu.code_address(address); }
    uint8_t read_byte(uint32_t address) override { return mmu.read_byte(address); }
};
