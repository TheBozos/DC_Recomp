// The original wrapper copies console output; the adapted wrapper retains its
// allocation extent, metadata, argument/result ABI and post-draw index reload.
#include "dc_functions.h"
#include "mdt_draw_ref_functions.h"
#include "mdt_draw_native_functions.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t visual = 0x400000, arena = 0x410000, source0 = 0x500000,
                       source1 = 0x510000, output = 0x520000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, copyCalls = 0, allocCalls = 0, drawCalls = 0;
    bool flip = false;
    uint32_t result = 0;
    std::array<uint32_t, 9> drawArgs{};
    RecompiledFunction alloc = nullptr;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void callback(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        if (ctx->pc == 0x127900) { ++allocCalls; alloc(ram, ctx, rt); return; }
        if (ctx->pc == 0x1033b0)
        {
            ++copyCalls;
            const uint32_t to = g.arg(0); g.copy(to, g.arg(1), g.arg(2)); g.retU(to);
        }
        else if (ctx->pc == 0x135000)
        {
            ++drawCalls;
            for (int i = 0; i < 8; ++i) drawArgs[i] = g.arg(i);
            drawArgs[8] = g.read32(g.arg(0) + 0x18);
            g.write32(visual + 0x38, result ^ 0x135000u);
            if (flip) g.write32(gp - 0x7440, g.read32(gp - 0x7440) ^ 1u);
            g.retU(result);
        }
        else throw std::runtime_error("unexpected MDT wrapper fixture call");
        ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context()
    {
        R5900Context ctx;
        for (int r = 16; r <= 31; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        for (int i = 0; i < 8; ++i)
        {
            ctx.r[4 + i].ud[0] = i ? 0x420000 + i * 0x1000 : visual;
            ctx.r[4 + i].ud[1] = 0x1234567800000000ull + i;
        }
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    alloc = rt->functions.generated(0x127900);
    for (uint32_t pc : {0x127900u, 0x1033b0u, 0x135000u}) rt->functions.setOverride(pc, callback);
    auto *ram = rt->memory.ram();
    for (uint32_t qwords : {0u, 1u, 31u, 64u})
        for (uint32_t offset : {0u, 1u, 4u, 5u})
            for (uint32_t frameIndex : {0u, 1u})
                for (bool copy : {false, true})
                    for (bool changeIndex : {false, true})
                        for (uint32_t returned : {0u, 57u, 0xffffffffu})
                        {
                            ++cases; flip = changeIndex; result = returned;
                            std::memset(ram + visual, 0, 0x40);
                            word(ram, visual + 0x1c, qwords); word(ram, visual + 0x24, copy);
                            word(ram, visual + 0x28, source0); word(ram, visual + 0x2c, source1);
                            word(ram, arena, output); word(ram, arena + 8, offset); word(ram, arena + 12, 0x1000);
                            word(ram, gp - 0x7464, arena); word(ram, gp - 0x7440, frameIndex);
                            for (uint32_t i = 0; i < 0x1000; ++i)
                            { ram[source0 + i] = static_cast<uint8_t>(i); ram[source1 + i] = static_cast<uint8_t>(i ^ 0x5a); }
                            std::memset(ram + output, 0xa5, 0x2000);
                            const std::vector<uint8_t> initialVisual(ram + visual, ram + visual + 0x40);
                            const std::vector<uint8_t> initialArena(ram + arena, ram + arena + 16);
                            copyCalls = allocCalls = drawCalls = 0;
                            auto original = context();
                            mdt_draw_ref_function_table[0].function(ram, &original, rt.get());
                            check(original.pc == 0x123450 && !original.leaving, "original wrapper returns normally");
                            check(copyCalls == copy && allocCalls == copy && drawCalls == 1, "reference allocates/copies only on copy flag");
                            const auto expectedArgs = drawArgs;
                            const std::vector<uint8_t> finalVisual(ram + visual, ram + visual + 0x40);
                            const std::vector<uint8_t> finalArena(ram + arena, ram + arena + 16);
                            std::memcpy(ram + visual, initialVisual.data(), initialVisual.size());
                            std::memcpy(ram + arena, initialArena.data(), initialArena.size());
                            word(ram, gp - 0x7440, frameIndex);
                            std::memset(ram + output, 0xa5, 0x2000);
                            std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                            std::memcpy(expectedRam.data() + visual, finalVisual.data(), finalVisual.size());
                            std::memcpy(expectedRam.data() + arena, finalArena.data(), finalArena.size());
                            word(expectedRam.data(), gp - 0x7440, frameIndex ^ static_cast<uint32_t>(flip));
                            auto native = context(); const auto before = native;
                            copyCalls = allocCalls = drawCalls = 0;
                            mdt_draw_native_function_table[0].function(ram, &native, rt.get());
                            check(native.pc == 0x123450 && !native.leaving, "adapted wrapper returns normally");
                            check(copyCalls == 0 && allocCalls == copy && drawCalls == 1, "native wrapper retains allocation and draw without copying");
                            check(drawArgs == expectedArgs, "all eight draw arguments and selected buffer metadata match");
                            check(native.r[2].ud[0] == original.r[2].ud[0], "original signed draw result preserved");
                            for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                                check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "callee-saved full registers/stack/return intact");
                            check(std::memcmp(ram, expectedRam.data(), stack - 0x1000) == 0 &&
                                  std::memcmp(ram + stack, expectedRam.data() + stack, expectedRam.size() - stack) == 0,
                                  "metadata and current-index restore match; no console output/other non-stack writes");
                        }
    // Buffers are metadata only, including the temporary allocation result.
    for (bool copy : {false, true})
    {
        ++cases; flip = false; result = 0;
        word(ram, visual + 0x1c, 64); word(ram, visual + 0x24, copy);
        word(ram, visual + 0x28, 0x12000000); word(ram, visual + 0x2c, 0x12000000);
        word(ram, arena, 0x12000000); word(ram, arena + 8, 0); word(ram, arena + 12, 0x1000);
        word(ram, gp - 0x7440, 0); copyCalls = 0;
        auto native = context();
        mdt_draw_native_function_table[0].function(ram, &native, rt.get());
        check(native.pc == 0x123450 && !copyCalls && drawArgs[8] == 0x12000000,
              "obsolete source and destination buffer addresses are never dereferenced");
    }
    std::printf("DC MDT wrapper: %d original/adapted/poison cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
