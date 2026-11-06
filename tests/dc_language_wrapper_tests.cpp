// Complete language loop, original vs exact guarded production adaptation.
// Packet and fade helpers execute their real compiled implementations; input
// processing and drawing are controlled boundaries with compared side effects.
#include "dc_functions.h"
#include "language_ref_functions.h"
#include "language_native_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace
{
    constexpr uint32_t packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000,
        fade = gp - 0x6818, language = gp - 0x6814, state = gp - 0x6810, scene = gp - 0x7968, marker = 0x420000;
    int cases = 0, failures = 0, packetCalls = 0;
    uint32_t procResult = 0;
    std::vector<BozoFunctionEntry> originals;
    struct Call
    {
        uint32_t pc = 0, arg = 0, value = 0;
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void callback(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx); const uint32_t pc = ctx->pc;
        if (pc == 0x120d70 || pc == 0x120c28 || pc == 0x2451f0 || pc == 0x245240)
        {
            if (pc == 0x120d70 || pc == 0x120c28)
            {
                ++packetCalls;
                if (pc == 0x120d70) check(g.arg(1) == 0x246100 && g.arg(2) == 0, "original static-chain identity and flags");
            }
            else calls.push_back({pc, g.arg(0), g.read32(g.arg(0))});
            for (const auto &entry : originals)
                if (entry.address == pc) { entry.function(ram, ctx, rt); return; }
            throw std::runtime_error("missing compiled language helper");
        }
        if (pc != 0x244d60 && pc != 0x244e40) throw std::runtime_error("unexpected language boundary");
        // These helpers take no arguments; record the logical state they see.
        calls.push_back({pc, g.read32(state), g.read32(language)});
        g.write32(marker, g.read32(marker) + pc);
        g.retU(pc == 0x244d60 ? procResult : 0x89abcdef);
        ctx->pc = ctx->r[31].uw[0];
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0 program"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context()
    {
        R5900Context ctx;
        for (int r = 16; r <= 31; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = 0x123450;
        ctx.fcr31 = 0x00800000;
        return ctx;
    }
    bool sameNonStack(const uint8_t *ram, const std::vector<uint8_t> &expected)
    {
        return std::memcmp(ram, expected.data(), stack - 0x1000) == 0 &&
            std::memcmp(ram + stack, expected.data() + stack, expected.size() - stack) == 0;
    }
}
int main()
{
    Host host; bozo::ee::Dispatcher dispatcher(host); auto &rt = dispatcher.runtime();
    rt.functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x120d70u, 0x120c28u, 0x2451f0u, 0x245240u})
        originals.push_back({pc, rt.functions.generated(pc)});
    for (uint32_t pc : {0x120d70u, 0x120c28u, 0x2451f0u, 0x245240u, 0x244d60u, 0x244e40u})
        rt.functions.setOverride(pc, callback);
    auto *ram = rt.memory.ram();
    for (uint32_t currentState : {0xffffffffu, 0u, 1u, 2u, 3u})
        for (int32_t currentFade : {-1, 0, 1, 4, 124, 127, 128, 129})
            for (uint32_t selected : {0u, 3u, 0x7fffffffu, 0xffffffffu})
                for (uint32_t result : {0u, 1u, 0xffffffffu})
                {
                    ++cases; procResult = result;
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output + 32 + (selected & 3u) * 4); word(ram, packet + 4, output);
                    word(ram, packet + 8, result ? output : 0); word(ram, gp - 0x742c, packet);
                    word(ram, fade, static_cast<uint32_t>(currentFade)); word(ram, language, selected);
                    word(ram, state, currentState); word(ram, scene, 0x76543210); word(ram, marker, 0);
                    std::memset(ram + 0x246100, 0xa5, 0x200); // static output is never read or changed here
                    std::memset(ram + stack - 0x1000, 0xa5, 0x1000);
                    const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
                    calls.clear(); packetCalls = 0;
                    auto original = context();
                    language_ref_function_table[0].function(ram, &original, &rt);
                    check(original.pc == 0x123450 && !original.leaving, "original returns normally");
                    check(packetCalls == 2, "original calls and terminates its static chain");
                    const auto expectedCalls = calls;
                    auto expected = initial;
                    std::memcpy(expected.data() + fade, ram + fade, 12);
                    std::memcpy(expected.data() + scene, ram + scene, 4);
                    std::memcpy(expected.data() + marker, ram + marker, 4);
                    std::memcpy(expected.data() + packet, ram + packet, 0x20000);
                    check(sameNonStack(ram, expected), "reference changes only logical fade/state/scene/callback effects and unused packets");
                    const bool completed = currentState == 2 && currentFade <= 0;
                    check(original.r[2].uw[0] == (completed ? 1u : 0u), "completion return contract");
                    if (completed)
                    {
                        uint32_t next; std::memcpy(&next, ram + scene, 4);
                        check(next == selected + 2u && calls.size() == 1, "completion publishes selected language and omits draw");
                    }
                    std::memcpy(ram, initial.data(), initial.size());
                    word(ram, gp - 0x742c, 0x12000000); std::memset(ram + packet, 0xa5, 0x20000);
                    word(expected.data(), gp - 0x742c, 0x12000000); std::memset(expected.data() + packet, 0xa5, 0x20000);
                    calls.clear(); packetCalls = 0;
                    auto native = context(); const auto before = native;
                    language_native_function_table[0].function(ram, &native, &rt);
                    check(native.pc == 0x123450 && !native.leaving, "adapted returns normally");
                    check(packetCalls == 0 && calls == expectedCalls, "same ordered fade/input/draw semantics without packet access");
                    check(native.r[2].ud[0] == original.r[2].ud[0], "complete native return value matches");
                    check(sameNonStack(ram, expected), "same logical effects; static chain/packet/other non-stack RAM untouched");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "full saved registers/stack/return preserved");
                    check(native.fcr31 == original.fcr31, "floating-point status matches");
                }
    std::printf("DC language loop: %d original/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
