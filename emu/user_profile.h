// Observe user instruction fetches and existing system calls; no guest changes.
// Machine adapters supply process-map identity and mapped data reads.
class UserProfileMemory {
public:
    virtual ~UserProfileMemory() = default;
    virtual uint32_t code_address(uint32_t address) const = 0;
    virtual uint8_t read_byte(uint32_t address) = 0;
};
class UserProfile {
    struct Process {
        std::string path, pending_path;
        unsigned start_sp = 65535, low_sp = 65535, high_break = 0;
        int pending = -1;
        unsigned requested = 0;
    } processes[128];
    z8000_device &cpu;
    UserProfileMemory &memory;
    FILE *file;
    void finish(unsigned seg) {
        auto &p = processes[seg];
        if (!p.path.empty()) {
            fprintf(file, "%u\t%s\t%u\t%u\t%u\n", seg, p.path.c_str(),
                    p.start_sp, p.low_sp, p.high_break);
            fflush(file);
        }
        p = Process();
    }
public:
    UserProfile(z8000_device &c, UserProfileMemory &m, FILE *f) : cpu(c), memory(m), file(f) {
        if (file) fprintf(file, "segment\tpath\tinitial_sp\tminimum_sp\tmaximum_break\n");
    }
    void sample(uint32_t address) {
        if (cpu.get_fcw() & 0x4000) return; // system/normal FCW bit
        address = memory.code_address(address);
        unsigned seg = (address >> 16) & 127;
        auto &p = processes[seg];
        if (p.pending == 11 || p.pending == 59) {
            // Successful exec starts the new image at zero; failure resumes
            // the old syscall wrapper. Preserve the old record on failure.
            if (!(address & 65535)) {
                std::string path = p.pending_path;
                finish(seg);
                p.path = path;
                p.start_sp = cpu.get_reg(15);
            }
            p.pending = -1;
        } else if (p.pending == 17) {
            if (cpu.get_reg(0) != 65535) {
                unsigned value = p.requested ? (p.requested + 63) & ~63u : cpu.get_reg(0);
                if (value > p.high_break) p.high_break = value;
            }
            p.pending = -1;
        }
        if (!p.path.empty() && cpu.get_reg(15) < p.low_sp)
            p.low_sp = cpu.get_reg(15);
    }
    static bool trap(void *context, uint8_t number, uint32_t pc) {
        auto &self = *static_cast<UserProfile *>(context);
        if (self.cpu.get_fcw() & 0x4000) return true;
        pc = self.memory.code_address(pc);
        unsigned seg = (pc >> 16) & 127;
        auto &p = self.processes[seg];
        if (number == 1) self.finish(seg);
        else if (number == 11 || number == 59) {
            p.pending = number;
            p.pending_path.clear();
            unsigned offset = self.cpu.get_reg(1);
            for (unsigned i = 0; i < 128; i++) {
                char ch = self.memory.read_byte((seg << 16) | ((offset+i) & 65535));
                if (!ch) break;
                p.pending_path += ch;
            }
        } else if (number == 17) {
            p.pending = number;
            p.requested = self.cpu.get_reg(1);
        }
        return true;
    }
    void flush() { for (unsigned seg = 0; seg < 128; seg++) finish(seg); }
};
