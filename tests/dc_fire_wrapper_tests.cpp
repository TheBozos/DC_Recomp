// Whole DrawFire before/after the exact guarded production adaptations.
// Camera/vector math, PRNG and packet helpers run their compiled originals.
// Texture lookup, projection and sprite submission are controlled boundaries.
#include "dc_functions.h"
#include "fire_ref_functions.h"
#include "fire_native_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t fire = 0x400000, camera = 0x410000, seed = 0x420000,
        marker = 0x430000, packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0, textureRequests = 0, projections = 0;
    uint32_t projectionMask = 0;
    uint64_t spriteCalls = 0, randomCalls = 0;
    std::vector<BozoFunctionEntry> originals;
    struct Call
    {
        uint32_t pc = 0;
        std::array<uint32_t, 24> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); }
    void number(uint8_t *ram, uint32_t at, float value) { word(ram, at, std::bit_cast<uint32_t>(value)); }
    void real(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        const uint32_t pc = ctx->pc;
        const bool packetHelper = pc == 0x120c80 || pc == 0x120e38 || pc == 0x120ec0 ||
            pc == 0x121028 || pc == 0x120ed8 || pc == 0x120e90;
        if (packetHelper) ++packetCalls;
        else
        {
            Call call{}; call.pc = pc;
            if (pc == 0x1046e8) call.data[0] = ctx->r[4].uw[0];
            calls.push_back(call);
            if (pc == 0x1046f8) ++randomCalls;
        }
        for (const auto &entry : originals)
            if (entry.address == pc) { entry.function(ram, ctx, rt); return; }
        throw std::runtime_error("unexpected real fire helper");
    }
    void boundary(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        if (ctx->pc == 0x1312d0)
        {
            for (int i = 0; i < 3; ++i) call.data[i] = g.arg(i);
            g.retU(0x440000 + 0x1000 * textureRequests++);
        }
        else if (ctx->pc == 0x12e740)
        {
            // Capture all semantic projection inputs, supply distinct corners
            // even on rejection to catch incorrect use of rejected projections.
            std::memcpy(call.data.data(), g.in(g.arg(2), 16), 16);
            call.data[4] = g.floatArg(0); call.data[5] = g.floatArg(1); call.data[6] = g.arg(3);
            for (uint32_t i = 0; i < 4; ++i)
            {
                g.write32(g.arg(0) + i * 4, 1000 + projections * 100 + i * 10);
                g.write32(g.arg(1) + i * 4, 2000 + projections * 100 + i * 10);
            }
            const bool accepted = (projectionMask & (1u << projections++)) != 0;
            g.ret(accepted ? 1 : 0);
        }
        else if (ctx->pc == 0x15dca0 || ctx->pc == 0x15d9a0)
        {
            // Native fog-sprite hooks never read argument zero (packet).
            call.data[0] = g.arg(1);
            std::memcpy(call.data.data() + 1, g.in(g.arg(2), 16), 16);
            const int points = ctx->pc == 0x15d9a0 ? 4 : 2;
            for (int i = 0; i < points; ++i)
                std::memcpy(call.data.data() + 5 + i * 4, g.in(g.arg(3 + i), 16), 16);
            call.data[21] = points == 4 ? g.arg(7) : g.read32(g.arg(5));
            ++spriteCalls; g.write32(marker, g.read32(marker) + 1); g.ret(0);
        }
        else throw std::runtime_error("unexpected fire boundary");
        calls.push_back(call); ctx->pc = ctx->r[31].uw[0];
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0 program"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *ctx, uint32_t at, uint32_t size, const uint64_t[2]) override { std::fprintf(stderr, "device write case %d pc %08x address %08x size %u\n", cases, ctx->pc, at, size); throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(uint32_t mode, float scale)
    {
        R5900Context ctx;
        for (int r = 16; r <= 23; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.fu[20] = 0x3eaaaaab; ctx.fu[21] = 0xc2f60000;
        ctx.fu[12] = std::bit_cast<uint32_t>(scale); ctx.fu[13] = std::bit_cast<uint32_t>(2.5f);
        ctx.r[4].uw[0] = fire; ctx.r[7].uw[0] = camera; ctx.r[9].uw[0] = mode;
        ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
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
    for (uint32_t pc : {0x120c80u, 0x120e38u, 0x120ec0u, 0x121028u, 0x120ed8u, 0x120e90u,
            0x124670u, 0x1246a0u, 0x1217b8u, 0x121648u, 0x1217e8u, 0x1046e8u, 0x1046f8u})
    {
        originals.push_back({pc, rt.functions.generated(pc)}); rt.functions.setOverride(pc, real);
    }
    for (uint32_t pc : {0x1312d0u, 0x12e740u, 0x15dca0u, 0x15d9a0u}) rt.functions.setOverride(pc, boundary);
    auto *ram = rt.memory.ram();
    for (uint32_t mode = 0; mode < 4; ++mode)
        for (int textureProfile = 0; textureProfile < 4; ++textureProfile)
            for (uint32_t counter : {0u, 3u, 4u, 0xffffffffu})
                for (projectionMask = 0; projectionMask < 8; ++projectionMask)
                {
                    ++cases;
                    std::memset(ram + fire, 0, 0x100);
                    number(ram, fire + 0x10, 1.25f); number(ram, fire + 0x14, 0.75f);
                    word(ram, fire + 0x18, counter == 3 ? 0x8001 : counter == 4 ? 0x7fff : 0);
                    word(ram, fire + 0x1c, counter);
                    number(ram, fire + 0x20, -12.25f); number(ram, fire + 0x24, 30.5f); number(ram, fire + 0x28, 54.75f);
                    word(ram, fire + 0x30, textureProfile == 3);
                    word(ram, fire + 0x34, textureProfile == 0 || textureProfile == 2 ? 0x460000 : 0);
                    word(ram, fire + 0x38, textureProfile == 0 ? 0x470000 : 0);
                    for (int i = 0; i < 4; ++i)
                    {
                        number(ram, camera + 0x260 + i * 4, i == 3 ? 1.0f : i * 10.0f + 1);
                        number(ram, camera + 0x270 + i * 4, i == 3 ? 1.0f : i * 20.0f - 2);
                    }
                    word(ram, 0x24fdec, seed); word(ram, seed + 0x58, 0xdeadbeef); word(ram, marker, 0);
                    number(ram, gp - 0x7f80, 0.5f); number(ram, gp - 0x7e14, -1.5f);
                    word(ram, gp - 0x78f8, 0x80604020);
                    word(ram, gp - 0x742c, packet);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output); word(ram, packet + 4, output);
                    std::memset(ram + stack - 0x1000, 0xa5, 0x1000);
                    const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
                    const float scale = textureProfile == 1 ? 0 : textureProfile == 2 ? -0.5f : 1.25f;
                    auto reference = context(mode, scale);
                    packetCalls = textureRequests = projections = 0; calls.clear();
                    fire_ref_function_table[0].function(ram, &reference, &rt);
                    check(reference.pc == 0x123450 && !reference.leaving, "original returns normally");
                    check(packetCalls == 9 + 9 * std::popcount(mode), "original executes only enabled state envelopes and final restore");
                    const auto expectedCalls = calls;
                    auto expectedReference = initial;
                    std::memcpy(expectedReference.data() + fire, ram + fire, 0x100);
                    std::memcpy(expectedReference.data() + seed, ram + seed, 0x100);
                    std::memcpy(expectedReference.data() + marker, ram + marker, 4);
                    std::memcpy(expectedReference.data() + packet, ram + packet, 0x20000);
                    check(sameNonStack(ram, expectedReference), "reference changes only fire/seed/sprite effects and unused packet output");
                    std::memcpy(ram, initial.data(), initial.size());
                    word(ram, gp - 0x742c, 0x12000000); // inaccessible packet pointer
                    std::memset(ram + packet, 0xa5, 0x20000);
                    auto expectedNative = expectedReference;
                    word(expectedNative.data(), gp - 0x742c, 0x12000000);
                    std::memset(expectedNative.data() + packet, 0xa5, 0x20000);
                    auto native = context(mode, scale); const auto before = native;
                    packetCalls = textureRequests = projections = 0; calls.clear();
                    fire_native_function_table[0].function(ram, &native, &rt);
                    check(native.pc == 0x123450 && !native.leaving, "native returns normally");
                    check(packetCalls == 0 && calls == expectedCalls, "same ordered camera/PRNG/projection/texture/sprite inputs, zero packet calls");
                    check(sameNonStack(ram, expectedNative), "same logical fire/PRNG/sprite state; no other non-stack changes");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "full saved registers/stack/return preserved");
                    check(native.fu[20] == before.fu[20] && native.fu[21] == before.fu[21], "saved float registers preserved");
                    check(native.fcr31 == reference.fcr31, "retained float status matches");
                }
    check(spriteCalls != 0 && randomCalls != 0, "fixtures reach native sprite and real compiled random paths");
    std::printf("DC fire: %d original/adapted cases, %llu sprite / %llu random calls, %d failures\n",
        cases, static_cast<unsigned long long>(spriteCalls), static_cast<unsigned long long>(randomCalls), failures);
    return failures ? 1 : 0;
}
