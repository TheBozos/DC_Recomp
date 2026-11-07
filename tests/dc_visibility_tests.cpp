// Original retail loop is extracted/generated locally; fixture data is synthetic.
#include "dc_functions.h"
#include "dcvis_functions.h"
#include "dc_visibility.h"
#include "bozo/libs/guest.h"
#include "bozo/port.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dc { bool nativeInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }
namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    constexpr uint32_t queries = 0x1c751c0, arena = 0x400000, buffer = 0x600000, gp = 0x300000;
    int failures = 0, cases = 0, mode = 0, reads = 0, readX = 0, readY = 0;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); }
    uint32_t sample(int32_t x, int32_t y)
    {
        const uint32_t a = static_cast<uint32_t>(x), b = static_cast<uint32_t>(y);
        switch (mode)
        {
        case 0: return 0;
        case 1: return 0xffffffff;
        case 2: return 0xa5000000u | ((a * 12347u + b * 997u) & 0x00ffffffu);
        case 3: return ((a + b * 5u) % 13u) == 0 ? 0x9b000000u : 0x00abcdefu;
        case 4: return 0x00ffffffu - ((a + b * 640u) & 0x00ffffffu);
        default: return (a * 0x9e3779b9u) ^ (b * 0x85ebca6bu);
        }
    }
    void sdk(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest guest(*rt, ctx);
        if (ctx->pc == 0x113498) // original scalar SetDefStoreImage arguments, no packet parsing
        {
            readX = static_cast<int16_t>(guest.arg(4));
            readY = static_cast<int16_t>(guest.arg(5));
            guest.fill(guest.arg(0), 0x6b, 96);
        }
        if (ctx->pc == 0x113758)
        {
            ++reads;
            std::array<uint32_t, 64> tile;
            for (int row = 0; row < 8; ++row)
                for (int col = 0; col < 8; ++col)
                    tile[row * 8 + col] = sample(readX + col, readY + row);
            guest.write(guest.arg(1), tile.data(), sizeof(tile));
        }
        guest.ret(0);
        ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context()
    {
        R5900Context ctx;
        ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = 0x700000; ctx.r[31].uw[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    const auto originalAlloc = rt->functions.generated(0x1278a0);
    rt->functions.setGenerated(dcvis_function_table, dcvis_function_count);
    rt->functions.setOverride(0x1278a0, originalAlloc);
    for (uint32_t pc : {0x1145a0u, 0x113498u, 0x113758u, 0x112f98u}) rt->functions.setOverride(pc, sdk);
    auto *ram = rt->memory.ram();
    constexpr std::array<uint32_t, 9> xs{0x80000000, 0xffffffff, 0, 3, 4, 5, 636, 637, 0x7fffffff};
    constexpr std::array<uint32_t, 9> ys{0xffffffff, 0, 3, 4, 5, 443, 444, 445, 0x7fffffff};
    for (mode = 0; mode < 6; ++mode)
        for (uint32_t seed = 0; seed < 24; ++seed)
        {
            ++cases;
            word(ram, gp - 0x7468, arena);
            word(ram, arena, buffer); word(ram, arena + 8, 123); word(ram, arena + 12, 0x800 + seed);
            for (uint32_t slot = 0; slot < 16; ++slot)
            {
                word(ram, queries + slot * 16, seed == 0 ? 0 : (slot + seed) % 4 == 0 ? 0 : 0x80000001);
                word(ram, queries + slot * 16 + 4, xs[(slot + seed) % xs.size()]);
                word(ram, queries + slot * 16 + 8, ys[(slot * 2 + seed) % ys.size()]);
                word(ram, queries + slot * 16 + 12, 0xabc00000 | slot);
            }
            const std::array<uint8_t, 256> initial = [&] {
                std::array<uint8_t, 256> a; std::memcpy(a.data(), ram + queries, a.size()); return a;
            }();
            reads = 0;
            auto original = context();
            rt->functions.generated(0x12d264)(ram, &original, rt.get());
            check(original.pc == 0x123450 && !original.leaving, "original isolated loop returns normally");
            std::array<uint8_t, 256> expected;
            std::memcpy(expected.data(), ram + queries, expected.size());
            uint32_t expectedExtent; std::memcpy(&expectedExtent, ram + arena + 8, 4);
            std::memcpy(ram + queries, initial.data(), initial.size());
            word(ram, arena, seed & 1 ? 0u : 0x12000000u); // no tile backing allocation is needed
            word(ram, arena + 8, 123);
            std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
            std::memcpy(expectedRam.data() + queries, expected.data(), expected.size());
            word(expectedRam.data(), arena + 8, expectedExtent);
            auto native = context(); const auto before = native;
            int samples = 0;
            dc::native::resolveVisibility(*rt, native, [&](int32_t x, int32_t y) { ++samples; return sample(x, y); });
            check(std::memcmp(ram, expectedRam.data(), expectedRam.size()) == 0,
                  "query results and arena extent match original; no packet/tile/other RAM writes");
            check(std::memcmp(&native, &before, sizeof(native)) == 0, "inline batch preserves complete CPU context");
            check(samples == reads * 64, "exactly one 8x8 native tile sampled per accepted query");
        }
    std::memset(ram + queries, 0, 256);
    auto ctx = context(); ctx.r[28].uw[0] = 0x12000000;
    const auto before = ctx;
    check(dc::nativeInstruction(ram, &ctx, rt.get(), 0x12d268), "guarded visibility site has a native handler");
    check(std::memcmp(&ctx, &before, sizeof(ctx)) == 0, "native site leaves PC/RA and all other context intact");
    word(ram, queries, 1); word(ram, queries + 4, 4); word(ram, queries + 8, 4);
    word(ram, arena + 12, 0x7ff); ctx = context();
    bool rejected = false;
    try { dc::native::resolveVisibility(*rt, ctx, sample); } catch (const std::runtime_error &) { rejected = true; }
    check(rejected, "undersized legacy arena fails explicitly instead of hanging or writing out of bounds");
    bozo::port::Registry registry;
    dc::native::install(*rt, registry);
    rejected = false; ctx.r[31].uw[0] = 0x12d3bc;
    try { registry.find(0x113758)(ram, &ctx, rt.get()); } catch (const std::runtime_error &) { rejected = true; }
    check(rejected, "retired SDK visibility readback cannot silently reappear");
    std::printf("DC visibility: %d original/native cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
