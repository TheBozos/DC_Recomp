// Synthetic manager/assets compared with the statically compiled original routines.
#include "dc_functions.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    int failures = 0, reloadCases = 0, loadCases = 0, fixedCases = 0, sdkCalls = 0;
    constexpr uint32_t manager = 0x400000, packet = 0x500000, buffer = 0x510000;
    constexpr uint32_t source = 0x600000, record = 0x480000, name = 0x610000;
    bozo::libs::Libraries libraries;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    R5900Context context()
    {
        R5900Context ctx;
        ctx.r[28].uw[0] = 0x300000;
        ctx.r[29].uw[0] = 0x700000;
        ctx.r[31].uw[0] = 0x123450;
        return ctx;
    }
    void sdk(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        const char *function = ctx->pc == 0x1132b0 ? "sceGsSetDefLoadImage" :
                               ctx->pc == 0x1135d8 ? "sceGsExecLoadImage" :
                               ctx->pc == 0x112f98 ? "sceGsSyncPath" : "FlushCache";
        ++sdkCalls;
        bozo::libs::Guest guest(*rt, ctx);
        // FlushCache is a kernel service, not a Libraries binding. This harness has
        // ordinary coherent host RAM and no device/cache side effects to reproduce.
        if (ctx->pc == 0x1145a0 || ctx->pc == 0x10c6f8) guest.ret(0);
        else libraries.find(function)->handler(libraries, guest);
        ctx->pc = ctx->r[31].uw[0];
    }
    void reload(Runtime &rt, bozo::port::Registry &registry)
    {
        auto *ram = rt.memory.ram();
        for (int group = -1; group <= 72; ++group)
            for (int state = 0; state < 3; ++state)
                for (int seed : {0, 1, 79})
                {
                    ++reloadCases;
                    std::memset(ram + manager, 0, 0x2000);
                    word(ram, manager, 8);
                    word(ram, manager + 0xc, state == 0 ? 0xffffffffu : static_cast<uint32_t>(group));
                    for (uint32_t i = 0; i < 72; ++i)
                    {
                        word(ram, manager + i * 60 + 0x3c, i * 32 + seed);
                        word(ram, manager + i * 60 + 0x40, state == 2);
                        word(ram, manager + i * 60 + 0x48, static_cast<uint32_t>(static_cast<int>(i * 17) - seed));
                    }
                    for (uint32_t i = 1; i < 8; ++i)
                    {
                        word(ram, manager + 0x10f8 + i * 80, static_cast<uint32_t>(group) & 0xffffu);
                        word(ram, manager + 0x10f8 + i * 80 + 0x28, i * 16);
                    }
                    const std::vector<uint8_t> initial(ram + manager, ram + manager + 0x2000);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, buffer); word(ram, packet + 4, buffer);
                    auto original = context();
                    original.r[4].uw[0] = manager; original.r[5].uw[0] = packet; original.r[6].sd[0] = group;
                    rt.functions.generated(0x133070)(ram, &original, &rt);
                    check(original.pc == 0x123450, "compiled reload returns normally");
                    const std::vector<uint8_t> expected(ram + manager, ram + manager + 0x2000);
                    std::memcpy(ram + manager, initial.data(), initial.size());
                    std::memset(ram + packet, 0xa5, 0x20000);
                    auto native = context(); native.pc = 0x133070;
                    native.r[4].uw[0] = manager; native.r[5].uw[0] = 0x12000000; native.r[6].sd[0] = group;
                    const int before = sdkCalls;
                    registry.find(0x133070)(ram, &native, &rt);
                    check(std::memcmp(ram + manager, expected.data(), expected.size()) == 0, "native group bookkeeping matches all manager bytes");
                    check(native.pc == 0x123450 && sdkCalls == before, "native reload returns without SDK traffic");
                    check(std::all_of(ram + packet, ram + packet + 0x20000, [](uint8_t b) { return b == 0xa5; }), "reload leaves old packet and output buffers untouched");
                }
    }
    void texture(Runtime &rt, bozo::port::Registry &registry, bool file, uint32_t kind, uint32_t width, uint32_t height, uint32_t base)
    {
        ++loadCases;
        auto *ram = rt.memory.ram();
        std::memset(ram + source, 0, 0x20000);
        std::memcpy(ram + name, "fixture", 8);
        std::memcpy(ram + 0x299bf0, "default", 8);
        uint32_t picture = source;
        if (file)
        {
            word(ram, source + 4, 1);
            std::memcpy(ram + source + 0x10, "fixture", 8);
            word(ram, source + 0x30, 0x200);
            picture += 0x200;
        }
        word(ram, picture + 0x18, width * height * 4);
        word(ram, picture + 0x1c, 0x30);
        word(ram, picture + 0x20, (kind << 24) | (1u << 8));
        word(ram, picture + 0x24, width | (height << 16));
        std::memset(ram + record, 0x5a, 0x50);
        const std::array<uint32_t, 5> args = file ? std::array<uint32_t, 5>{name, source, record, base, base + 31} :
                                                               std::array<uint32_t, 5>{source, record, base, base + 31, 0};
        auto original = context();
        for (unsigned i = 0; i < args.size(); ++i) original.r[4+i].ud[0] = args[i];
        const uint32_t pc = file ? 0x154770 : 0x154950;
        rt.functions.generated(pc)(ram, &original, &rt);
        check(original.pc == 0x123450, "compiled texture load returns normally");
        std::array<uint8_t, 0x50> expected;
        std::memcpy(expected.data(), ram + record, expected.size());
        std::memset(ram + record, 0x5a, 0x50);
        std::memset(ram + packet, 0xa5, 0x20000);
        auto native = context(); native.pc = pc;
        for (unsigned i = 0; i < args.size(); ++i) native.r[4+i].ud[0] = args[i];
        const int before = sdkCalls;
        registry.find(pc)(ram, &native, &rt);
        check(std::memcmp(ram + record, expected.data(), expected.size()) == 0, "native texture load matches all record bytes");
        check(native.pc == 0x123450 && native.r[31].uw[0] == 0x123450 && sdkCalls == before, "native texture load returns without upload/cache SDK calls");
        check(std::all_of(ram + packet, ram + packet + 0x20000, [](uint8_t b) { return b == 0xa5; }), "texture load leaves old packet output untouched");
    }
    void fixed(Runtime &rt, bozo::port::Registry &registry, const std::array<uint64_t, 14> &args, uint32_t pc)
    {
        ++fixedCases;
        auto *ram = rt.memory.ram();
        std::memset(ram + manager, 0, 0x5000);
        word(ram, manager, 12);
        word(ram, manager + 20, 0x3f00);
        std::memcpy(ram + manager + 0x1148 + 8, "occupied", 9);
        const std::vector<uint8_t> initial(ram + manager, ram + manager + 0x5000);
        auto make = [&] {
            auto ctx = context();
            for (unsigned i = 0; i < args.size(); ++i)
                if (i < 8) ctx.r[4+i].ud[0] = args[i];
                else std::memcpy(ram + ctx.r[29].uw[0] + (i-8)*8, &args[i], 8);
            return ctx;
        };
        auto original = make();
        rt.functions.generated(pc)(ram, &original, &rt);
        check(original.pc == 0x123450, "compiled fixed texture returns normally");
        const std::vector<uint8_t> expected(ram + manager, ram + manager + 0x5000);
        std::memcpy(ram + manager, initial.data(), initial.size());
        std::memset(ram + 0x1c7a830, 0xa5, 0x4000);
        auto native = make(); native.pc = pc;
        const int before = sdkCalls;
        registry.find(pc)(ram, &native, &rt);
        check(std::memcmp(ram + manager, expected.data(), expected.size()) == 0, "fixed texture matches all original manager/record bytes");
        check(native.pc == 0x123450 && sdkCalls == before, "fixed texture returns without upload/cache/sync calls");
        check(std::all_of(ram + 0x1c7a830, ram + 0x1c7e830, [](uint8_t b) { return b == 0xa5; }), "fixed texture leaves console packet buffer intact");
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x1132b0u, 0x1135d8u, 0x1145a0u, 0x10c6f8u, 0x112f98u}) rt->functions.setOverride(pc, sdk);
    bozo::port::Registry registry;
    dc::native::install(*rt, registry);
    reload(*rt, registry);
    for (bool file : {false, true})
        for (uint32_t kind = 1; kind <= 5; ++kind)
            for (uint32_t width : {8u, 64u})
                for (uint32_t base : {0u, 0x1180u, 0xffffu})
                    texture(*rt, registry, file, kind, width, 8, base);
    auto *ram = rt->memory.ram();
    std::memcpy(ram + name, "fixture", 8);
    for (uint32_t kind : {1u, 3u, 4u})
        for (bool pixels : {false, true})
            for (uint32_t mipMode : {0u, 1u, 2u})
                for (uint32_t width : {8u, 64u, 65u})
                    for (uint64_t filter : {0ull, 0xabcdef1234567890ull})
                        for (uint32_t order : {0u, 1u})
                            fixed(*rt, registry, {manager, name, pixels ? source : 0u, width, 8, kind, source + 0x1000u, 0,
                                  mipMode != 0, mipMode == 2 ? source + 0x2000u : 0u, source + 0x3000u, 0, filter, order}, 0x1320e0);
    std::memset(ram + source, 0, 0x400);
    std::memcpy(ram + source + 0x10, "fixtureZ", 9);
    word(ram, source + 0x30, 0x100);
    for (uint32_t kind : {3u, 5u})
        for (uint32_t width : {640u, 320u})
            for (uint32_t height : {224u, 112u})
            {
                word(ram, source + 0x124, width | height << 16);
                word(ram, source + 0x120, kind << 24);
                word(ram, source + 0x11c, 0x30);
                word(ram, source + 0x118, 640*224);
                fixed(*rt, registry, {manager, source}, 0x132930);
            }
    std::printf("DC native texture services: %d reload cases, %d load cases, %d fixed cases, %d failures\n", reloadCases, loadCases, fixedCases, failures);
    return failures ? 1 : 0;
}
