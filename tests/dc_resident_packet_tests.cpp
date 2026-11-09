// Complete init/begin-frame/loading owners, plus the exact main-loop envelope.
// Native effects are controlled boundaries; retained math and packet SDK run
// compiled originals. Production patches are the only before/after difference.
#include "dc_functions.h"
#if defined(DC_PACKET_INIT)
#include "packet_init_ref_functions.h"
#include "packet_init_native_functions.h"
#define REFERENCE packet_init_ref_function_table
#define ADAPTED packet_init_native_function_table
constexpr int kind = 0;
#elif defined(DC_BEGIN_FRAME)
#include "begin_frame_ref_functions.h"
#include "begin_frame_native_functions.h"
#define REFERENCE begin_frame_ref_function_table
#define ADAPTED begin_frame_native_function_table
constexpr int kind = 1;
#elif defined(DC_RESIDENT_CHAIN)
#include "resident_chain_ref_functions.h"
#include "resident_chain_native_functions.h"
#define REFERENCE resident_chain_ref_function_table
#define ADAPTED resident_chain_native_function_table
constexpr int kind = 2;
#else
#include "loading_init_ref_functions.h"
#include "loading_init_native_functions.h"
#define REFERENCE loading_init_ref_function_table
#define ADAPTED loading_init_native_function_table
constexpr int kind = 3;
#endif
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace
{
    constexpr uint32_t gp = 0x300000, stack = 0x700000, output = 0x500000,
        packets = 0x1c7a780, loadingPacket = 0x1cfb370, marker = 0x480000;
    int failures = 0, cases = 0, packetCalls = 0, loadResult = 0;
    uint64_t retainedCalls = 0;
    std::vector<BozoFunctionEntry> originals;
    struct Event
    {
        uint32_t pc = 0;
        std::array<uint32_t, 8> data{};
        bool operator==(const Event &) const = default;
    };
    std::vector<Event> events;
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void number(uint8_t *ram, uint32_t at, float v) { word(ram, at, std::bit_cast<uint32_t>(v)); }
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL resident %d case %d: %s\n", kind, cases, what); }
    }
    void callback(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx); const auto pc = ctx->pc;
        if (pc == 0x120c08 || pc == 0x120c18 || pc == 0x120d70)
        {
            ++packetCalls;
            for (auto entry : originals) if (entry.address == pc) { entry.function(ram, ctx, rt); return; }
            throw std::runtime_error("missing original packet builder");
        }
        Event event{}; event.pc = pc;
        if (pc == 0x1110b0 || pc == 0x121830 || pc == 0x1218a8 || pc == 0x12c700)
        {
            if (pc == 0x1110b0) event.data[0] = std::bit_cast<uint32_t>(ctx->f[12]);
            else
            {
                event.data[0] = g.arg(0);
                if (pc == 0x121830) event.data[1] = g.arg(1);
            }
            events.push_back(event); ++retainedCalls;
            for (auto entry : originals) if (entry.address == pc) { entry.function(ram, ctx, rt); return; }
            throw std::runtime_error("missing retained math/callback helper");
        }
        switch (pc)
        {
        case 0x140da0: break; // SetEnv's native effect ignores its packet argument.
        case 0x130000:
            for (int i = 0; i < 4; ++i) event.data[i] = g.arg(i);
            break;
        case 0x12fd30: event.data[0] = g.arg(0); break;
        case 0x105058:
            // String formatting is outside this packet change. Provide a
            // deterministic nonempty asset path, including its terminating NUL.
            event.data[0] = g.arg(0); event.data[1] = g.arg(1);
            event.data[2] = g.arg(2); event.data[3] = g.arg(3);
            g.write(g.arg(0), "fixture.tex", 12); break;
        case 0x13f3d0:
            for (int i = 0; i < 4; ++i) event.data[i] = g.arg(i);
            g.write32(g.arg(2), 64); break;
        case 0x154770:
        case 0x154950:
            for (int i = 0; i < (pc == 0x154770 ? 5 : 4); ++i) event.data[i] = g.arg(i);
            // Stand-in for the native asset publication's logical effect.
            g.write32(g.arg(pc == 0x154770 ? 2 : 1), pc); break;
        case 0x112c08:
            for (int i = 0; i < 7; ++i) event.data[i] = g.arg(i);
            g.write32(g.arg(0), 0x12345678); break;
        default: throw std::runtime_error("unexpected resident fixture boundary");
        }
        events.push_back(event); ++retainedCalls;
        g.write32(marker, g.read32(marker) * 33 + pc);
        g.ret(pc == 0x13f3d0 ? loadResult : 0); ctx->pc = ctx->r[31].uw[0];
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(unsigned phase, unsigned mode)
    {
        R5900Context ctx;
        for (int r = 16; r < 32; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = 0x123450;
        ctx.r[4].sd[0] = kind == 0 ? int32_t(phase == 3 ? 0xfffffffdu : output + phase) :
            kind == 3 ? std::array<int,6>{0,4,5,99,100,801}[phase] : 0;
        ctx.r[5].sd[0] = int32_t(output + 0x8000 + mode);
        for (int f = 20; f <= 23; ++f) ctx.f[f] = std::bit_cast<float>(0x43210000u + f);
        return ctx;
    }
    void loadElf(uint8_t *ram, const char *path)
    {
        std::ifstream file(path, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        auto u32 = [&](size_t at) { uint32_t v; if (at + 4 > bytes.size()) throw std::runtime_error("invalid fixture ELF"); std::memcpy(&v, bytes.data() + at, 4); return v; };
        if (u32(0) != 0x464c457f) throw std::runtime_error("missing fixture ELF");
        for (uint32_t i = 0; i < (u32(44) & 0xffff); ++i)
        {
            const auto at = u32(28) + i * 32;
            if (u32(at) != 1) continue;
            const auto offset = u32(at + 4), address = u32(at + 8), size = u32(at + 16), memory = u32(at + 20);
            if (size > memory || offset + uint64_t(size) > bytes.size() || address + uint64_t(memory) > bozo::kFlatScratchpadOffset)
                throw std::runtime_error("invalid fixture segment");
            std::memset(ram + address, 0, memory); std::memcpy(ram + address, bytes.data() + offset, size);
        }
    }
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    Host host; bozo::ee::Dispatcher dispatcher(host); auto &rt = dispatcher.runtime();
    rt.functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x120c08u,0x120c18u,0x120d70u,0x1110b0u,0x121830u,0x1218a8u,0x12c700u})
        originals.push_back({pc, rt.functions.generated(pc)});
    for (uint32_t pc : {0x120c08u,0x120c18u,0x120d70u,0x1110b0u,0x121830u,0x1218a8u,0x12c700u,
         0x12fd30u,0x130000u,0x140da0u,0x105058u,0x112c08u,0x13f3d0u,0x154770u,0x154950u})
        rt.functions.setOverride(pc, callback);
    auto *ram = rt.memory.ram(); loadElf(ram, argv[1]);
    const std::vector<uint8_t> base(ram, ram + bozo::kFlatScratchpadOffset);
    for (unsigned phase = 0; phase < (kind == 3 ? 6u : 4u); ++phase)
        for (unsigned mode = 0; mode < 4; ++mode)
            for (unsigned gate = 0; gate < 4; ++gate)
            {
                ++cases; loadResult = gate & 1;
                std::memcpy(ram, base.data(), base.size());
                std::memset(ram + stack - 0x10000, 0xa5, 0x10000);
                std::memset(ram + output, 0, 0x20000);
                std::memset(ram + packets, 0x5a, 64); std::memset(ram + loadingPacket, 0x5a, 32);
                for (uint32_t object : {packets, packets + 32, loadingPacket})
                {
                    word(ram, object, output + 32 + mode * 4); word(ram, object + 4, output);
                    word(ram, object + 8, mode & 1 ? output : 0);
                }
                word(ram, gp - 0x742c, packets + (phase & 1) * 32); word(ram, gp - 0x7440, phase & 1);
                word(ram, gp - 0x7424, gate & 1); // begin-frame clear mode
                word(ram, gp - 0x7468, 0x410000); word(ram, marker, 0);
                word(ram, gp - 0x721c, gate >> 1); word(ram, gp - 0x7968, mode);
                for (uint32_t i = 0; i < 4; ++i) number(ram, 0x1c75500 + i * 4, std::array<float,4>{-1.f,0.f,127.5f,255.f}[phase % 4] + i);
                const std::vector<uint8_t> initial(ram, ram + base.size());
                auto original = context(phase, mode); events.clear(); packetCalls = 0;
                REFERENCE[0].function(ram, &original, &rt);
                check(original.pc == 0x123450 && !original.leaving, "reference returns normally");
                check(packetCalls == (kind == 0 ? 4 : kind == 3 ? ((gate >> 1) || !(gate & 1) ? 0 : 1) : 1), "reference conditional packet count");
                auto expectedEvents = events;
                std::vector<uint8_t> expected(ram, ram + base.size());
                if (kind == 3)
                {
                    // The loading screen's sceGsSetDefDBuff is retired (its records are read only by the callback
                    // region already skipped): no event, no write into the record, and the marker folds without it.
                    std::vector<Event> kept;
                    const auto read = [&](uint32_t at) { uint32_t v; std::memcpy(&v, initial.data() + at, 4); return v; };
                    uint32_t folded = read(marker);
                    for (const auto &event : expectedEvents)
                    {
                        if (event.pc == 0x112c08)
                        {
                            word(expected.data(), event.data[0], read(event.data[0]));
                            continue;
                        }
                        kept.push_back(event);
                        if (event.pc != 0x1110b0 && event.pc != 0x121830 && event.pc != 0x1218a8 && event.pc != 0x12c700)
                            folded = folded * 33 + event.pc;
                    }
                    word(expected.data(), marker, folded);
                    expectedEvents = kept;
                }
                std::memcpy(ram, initial.data(), initial.size());
                // Poison both main objects, the loading object and all retired
                // output. Main's packet handle is deliberately inaccessible.
                word(ram, gp - 0x742c, 0x12000000);
                std::memset(ram + packets, 0xa5, 64); std::memset(ram + loadingPacket, 0xa5, 32);
                std::memset(ram + output, 0xa5, 0x20000);
                // The packet handle is now retired in begin-frame too.
                word(expected.data(), gp - 0x742c, 0x12000000);
                std::memset(expected.data() + packets, 0xa5, 64); std::memset(expected.data() + loadingPacket, 0xa5, 32);
                std::memset(expected.data() + output, 0xa5, 0x20000);
                auto native = context(phase, mode); const auto before = native; events.clear(); packetCalls = 0;
                ADAPTED[0].function(ram, &native, &rt);
                check(native.pc == 0x123450 && !native.leaving, "adapted returns normally");
                check(packetCalls == 0 && events == expectedEvents, "same ordered native effects with no packet SDK");
                check(std::memcmp(ram, expected.data(), stack - 0x10000) == 0 &&
                    std::memcmp(ram + stack, expected.data() + stack, expected.size() - stack) == 0,
                    "same logical RAM; packet objects/output untouched");
                for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                    if (!(kind == 2 && (r == 20 || r == 23)))
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "full saved registers preserved");
                for (int f = 20; f <= 23; ++f) check(native.f[f] == before.f[f], "saved FPU registers preserved");
                check(native.fcr31 == original.fcr31, "float status retained");
            }
    if (kind != 0) check(retainedCalls != 0, "reached retained native/math/callback boundary");
    std::printf("DC resident %d: %d original/adapted cases, %d failures\n", kind, cases, failures);
    return failures ? 1 : 0;
}
