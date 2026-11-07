// Export the pinned compiler's indirect-control facts; no retail bytes emitted.
#include "analysis.h"
#include "elf.h"
#include "program.h"
#include <fstream>
#include <iostream>

namespace
{
    // An origin hint, not reaching-definition or alias proof. Stop at control
    // flow and describe the last local definition only when it is unambiguous.
    int written(const bozo::Insn &i)
    {
        const auto op = i.opcode();
        if ((op >= 8 && op <= 15) || (op >= 24 && op <= 27) ||
            (op >= 32 && op <= 39) || op == 30 || op == 55 ||
            ((op == 16 || op == 17 || op == 18) && i.rs() <= 2)) return i.rt();
        if (op == 0 || op == 28) return i.rd();
        return -1;
    }
}
int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    bozo::ElfFile elf;
    if (!elf.load(argv[1])) { std::cerr << elf.error(); return 1; }
    auto program = bozo::programFromElf(elf);
    std::vector<bozo::InstructionPatch> patches; std::string error;
    if (!bozo::readInstructionPatches(argv[2], patches, error) ||
        !bozo::applyInstructionPatches(program, patches, error)) { std::cerr << error; return 1; }
    bozo::Analysis analysis(program); analysis.run();
    std::ofstream out(argv[3]);
    out << "{\"schema\":1,\"functions\":[";
    bool comma = false;
    for (const auto &f : analysis.functions())
    {
        if (comma) out << ','; comma = true;
        out << "{\"start\":" << f.start << ",\"end\":" << f.end << ",\"entries\":[";
        bool entryComma = false;
        for (uint32_t entry : f.extraEntries) { if (entryComma) out << ','; entryComma = true; out << entry; }
        out << "],\"sites\":[";
        bool siteComma = false;
        for (uint32_t pc = f.start; pc + 4 <= f.end; pc += 4)
        {
            const auto i = bozo::decodeAt(program.read32(pc), pc);
            if (i.op != bozo::Op::JR && i.op != bozo::Op::JALR) continue;
            if (siteComma) out << ','; siteComma = true;
            const auto table = f.jumpTables.find(pc);
            const char *kind = i.op == bozo::Op::JALR ? "call_register" : i.rs() == 31 ? "return" :
                f.returnJr.contains(pc) ? "compiler_return_copy" : table != f.jumpTables.end() ? "compiler_switch" : "jump_register";
            out << "{\"pc\":" << pc << ",\"reg\":" << i.rs() << ",\"kind\":\"" << kind << '"';
            if (table != f.jumpTables.end())
            {
                out << ",\"table\":" << table->second.tableAddress << ",\"in_place\":"
                    << (table->second.inPlace ? "true" : "false") << ",\"targets\":[";
                bool targetComma = false;
                for (auto target : table->second.targets) { if (targetComma) out << ','; targetComma = true; out << target; }
                out << ']';
            }
            for (uint32_t before = pc, limit = 16; before > f.start && limit--;)
            {
                before -= 4;
                const auto def = bozo::decodeAt(program.read32(before), before);
                if (bozo::opFlags(def.op) & (bozo::BR | bozo::JMP | bozo::LNK)) break;
                if (written(def) != static_cast<int>(i.rs())) continue;
                out << ",\"definition\":" << before << ",\"definition_op\":\"" << bozo::opName(def.op) << '"';
                if (def.op == bozo::Op::LW || def.op == bozo::Op::LWU || def.op == bozo::Op::LD)
                    out << ",\"slot_base_reg\":" << def.rs() << ",\"slot_offset\":" << def.simm();
                break;
            }
            out << '}';
        }
        out << "]}";
    }
    out << "]}\n"; out.close();
    return out ? 0 : 1;
}
