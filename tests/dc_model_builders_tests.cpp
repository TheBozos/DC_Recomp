// Synthetic assets compared with the statically compiled original builders. No game
// assets are embedded here; generated retail code is available only in a local build.
#include "dc_functions.h"
#include "bozo/port.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    int failures = 0, cases = 0;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    constexpr uint32_t visual = 0x400000, asset = 0x410000, output = 0x500000;
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    uint32_t word(const uint8_t *ram, uint32_t at) { uint32_t v; std::memcpy(&v, ram + at, 4); return v; }
    R5900Context context(uint32_t destination = output)
    {
        R5900Context ctx;
        ctx.r[4].uw[0] = visual;
        ctx.r[5].uw[0] = destination;
        ctx.r[6].uw[0] = asset;
        ctx.r[7].uw[0] = 1;
        ctx.r[28].uw[0] = 0x300000;
        ctx.r[29].uw[0] = 0x700000;
        ctx.r[31].uw[0] = 0x123450;
        return ctx;
    }
    void mdt(uint8_t *ram, bool colours, uint32_t kind, int material, const std::vector<uint32_t> &counts)
    {
        std::memset(ram + asset, 0, 0x10000);
        word(ram, asset, 0x0054444d);
        word(ram, asset + 8, 0x10000);
        for (auto [offset, value] : {std::pair{0x10u, 0x100u}, {0x18u, 0x200u}, {0x20u, colours ? 0x400u : 0u},
                                    {0x28u, 0x1000u}, {0x30u, 0x300u}, {0x38u, 0x800u}})
            word(ram, asset + offset, value);
        std::memcpy(ram + asset + 0x834, "fixture", 8);
        word(ram, asset + 0x1008, static_cast<uint32_t>(counts.size()));
        uint32_t at = asset + 0x1010;
        for (auto n : counts)
        {
            word(ram, at, kind); word(ram, at + 4, n); word(ram, at + 8, static_cast<uint32_t>(material));
            at += 12 + n * (colours ? 16 : 12);
        }
    }
    void compare(Runtime &rt, bozo::port::Registry &registry, uint32_t pc)
    {
        ++cases;
        auto *ram = rt.memory.ram();
        std::memset(ram + visual, 0x5a, 0x80);
        std::memset(ram + output, 0xa5, 0x20000);
        auto original = context();
        rt.functions.generated(pc)(ram, &original, &rt);
        check(original.pc == 0x123450 && !original.leaving, "original returns normally");
        const uint32_t expectedSize = original.r[2].uw[0];
        const uint32_t expectedVisualSize = word(ram, visual + 0x1c);
        std::memset(ram + visual, 0x5a, 0x80);
        std::memset(ram + output, 0xa5, 0x20000);
        auto native = context(); native.pc = pc;
        registry.find(pc)(ram, &native, &rt);
        // The native extent is one qword: CreateVisual reserves it at the data address and aligns to 64 bytes, so each
        // build keeps a distinct address. The original's VIF packet size is no longer reserved.
        check(expectedSize > 1u && expectedVisualSize == expectedSize, "the original reserves its VIF packet size");
        check(native.r[2].uw[0] == 1u, "native allocation extent is one qword");
        check(word(ram, visual + 0x1c) == 1u && word(ram, visual + 0x18) == output,
              "visual records its data address and the native extent");
        check(native.pc == 0x123450 && native.r[29].uw[0] == 0x700000, "native builder preserves return and stack");
        check(std::all_of(ram + output, ram + output + 0x20000, [](uint8_t v) { return v == 0xa5; }),
              "native builder never serializes the console output");
        native = context(0x12000000); native.pc = pc;
        registry.find(pc)(ram, &native, &rt);
        check(native.r[2].uw[0] == 1u, "obsolete output address is never dereferenced");
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    bozo::port::Registry registry;
    dc::native::install(*rt, registry);
    auto *ram = rt->memory.ram();
    std::memcpy(ram + 0x1c75870 + 0x1100, "fixture", 8);
    for (bool textureMode : {false, true})
        for (bool defaultMode : {false, true})
        {
            word(ram, 0x1c75870 + 0x10f8 + 0x30, textureMode ? 1u : 0u);
            word(ram, 0x300000 - 0x7420, defaultMode ? 1u : 0u);
            for (bool colours : {false, true})
                for (uint32_t kind : {3u, 4u})
                    for (int material : {-1, 0})
                        for (uint32_t count : {0u, 1u, 3u, 31u, 32u, 33u, 34u, 53u, 54u, 55u, 65u, 108u, 160u})
                        {
                            mdt(ram, colours, kind, material, {count, 3, count});
                            compare(*rt, registry, 0x135aa0);
                            compare(*rt, registry, 0x135e50);
                        }
            for (uint32_t count : {0u, 3u, 27u, 30u, 33u, 60u, 63u, 300u})
            {
                mdt(ram, false, 3, -1, {count, 3, count});
                compare(*rt, registry, 0x136530);
            }
            for (bool namedTexture : {false, true})
                for (uint32_t rows : {0u, 1u, 2u, 8u, 16u})
                    for (uint32_t columns : {0u, 1u, 2u, 8u, 16u})
                    {
                        word(ram, visual + 0x2c, rows); word(ram, visual + 0x30, columns);
                        std::memset(ram + visual + 0x94, 0, 32);
                        if (namedTexture) std::memcpy(ram + visual + 0x94, "fixture", 8);
                        // compare() initializes the visual; cloth dimensions need separate setup.
                        auto original = context();
                        rt->functions.generated(0x13c5f0)(ram, &original, rt.get());
                        const std::vector<uint8_t> beforeCloth(ram, ram + bozo::kFlatScratchpadOffset);
                        auto native = context(0x12000000); native.pc = 0x13c5f0;
                        registry.find(0x13c5f0)(ram, &native, rt.get());
                        ++cases;
                        check(original.r[2].uw[0] > 1u && native.r[2].uw[0] == 1u,
                              "the original reserves its VIF size; native cloth reserves one qword");
                        check(std::memcmp(ram, beforeCloth.data(), beforeCloth.size()) == 0, "cloth builder leaves all guest RAM unchanged");
                    }
        }
    mdt(ram, false, 3, -1, {3});
    word(ram, asset + 0x1014, 0xffffffff);
    const std::vector<uint8_t> before(ram, ram + bozo::kFlatScratchpadOffset);
    auto ctx = context();
    bool rejected = false;
    try { registry.find(0x135aa0)(ram, &ctx, rt.get()); }
    catch (const std::runtime_error &) { rejected = true; }
    check(rejected, "invalid MDT rejected");
    check(std::memcmp(ram, before.data(), before.size()) == 0, "invalid MDT leaves all guest RAM unchanged");
    std::printf("DC native asset builders: %d cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
