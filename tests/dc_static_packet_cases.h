// Exact retail spans around static VIF/VU1 packet calls. The synthetic harness
// uses s7 for its return address, as the existing frame-span fixtures do.
#include "dc_functions.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace
{
    constexpr uint32_t packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0;
    std::vector<BozoFunctionEntry> packetFunctions;
    struct Call
    {
        uint32_t pc, arg;
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
        bozo::libs::Guest g(*rt, ctx);
        if (ctx->pc == 0x12e280) { ++packetCalls; g.retU(g.read32(gp - 0x742c)); }
        else if (ctx->pc == 0x120d70 || ctx->pc == 0x120c28)
        {
            ++packetCalls;
            for (const auto &entry : packetFunctions)
                if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
            throw std::runtime_error("missing original packet helper");
        }
        else
        {
            const auto pc = ctx->pc;
            if (pc != 0x177fa0 && pc != 0x22bdf0 && pc != 0x22d0e0)
                throw std::runtime_error("unexpected static-chain fixture callback");
            // EdSetFlag has no arguments; menu camera/reload each have one.
            const uint32_t arg = pc == 0x177fa0 ? 0u : g.arg(0);
            calls.push_back({pc, arg});
            g.write32(0x420000 + static_cast<uint32_t>(calls.size()) * 4, pc ^ arg);
            g.retU(0x12345678);
        }
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
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x120d70u, 0x120c28u})
    {
        packetFunctions.push_back({pc, rt->functions.generated(pc)});
        rt->functions.setOverride(pc, callback);
    }
    for (uint32_t pc : {0x12e280u, 0x177fa0u, 0x22bdf0u, 0x22d0e0u}) rt->functions.setOverride(pc, callback);
    auto *ram = rt->memory.ram();
    for (uint32_t alignment = 0; alignment < 4; ++alignment)
        for (bool open : {false, true})
            for (uint32_t marker : {0u, 1u, 0x7fffffffu, 0xffffffffu})
            {
                ++cases;
                std::memset(ram + packet, 0, 0x20000);
                word(ram, packet, output + 32 + alignment * 4); word(ram, packet + 4, output);
                word(ram, packet + 8, open ? output : 0);
                word(ram, gp - 0x742c, packet); word(ram, gp - 0x6b20, marker);
                for (auto offset : {0x789cu, 0x6f18u, 0x6f00u, 0x6ee8u}) word(ram, gp - offset, marker ^ offset);
                std::memset(ram + 0x1d19bb0, 0xa5, 0x20);
                std::memset(ram + 0x420000, 0xa5, 16);
                calls.clear(); packetCalls = 0;
                auto original = context();
                DC_STATE_REFERENCE[0].function(ram, &original, rt.get());
                const auto expectedCalls = calls;
                check(original.pc == 0x123450 && !original.leaving, "original span returns normally");
#ifdef DC_EDIT_STATE
                check(packetCalls == 1 && calls.size() == 1 && calls[0].pc == 0x177fa0, "reference calls packet and retained EdSetFlag once");
#else
                check(packetCalls == 4 && calls.size() == 2 && calls[0].pc == 0x22bdf0 && calls[1].pc == 0x22d0e0,
                      "reference looks up packet twice and calls/terminates between camera and reload");
#endif
                const std::vector<uint8_t> finalScene(ram + 0x1d19bb0, ram + 0x1d19bd0);
                const std::vector<uint8_t> finalLogical(ram + 0x420000, ram + 0x420010);
                std::memset(ram + 0x1d19bb0, 0xa5, 0x20); std::memset(ram + 0x420000, 0xa5, 16);
                std::memset(ram + packet, 0xa5, 0x20000); word(ram, gp - 0x742c, 0x12000000);
                std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                std::memcpy(expectedRam.data() + 0x1d19bb0, finalScene.data(), finalScene.size());
                std::memcpy(expectedRam.data() + 0x420000, finalLogical.data(), finalLogical.size());
                auto native = context(); const auto before = native;
                calls.clear(); packetCalls = 0;
                DC_STATE_NATIVE[0].function(ram, &native, rt.get());
                check(native.pc == 0x123450 && !native.leaving, "native span returns normally");
                check(packetCalls == 0 && calls == expectedCalls, "retained callbacks/arguments/order match with no packet helpers");
                check(native.r[2].ud[0] == original.r[2].ud[0], "retained final callback result matches");
                for (int r : {16,17,18,19,20,21,22,28,29,30,31})
                    check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "saved registers/stack/return preserved (s7 is harness scratch)");
                check(std::memcmp(ram, expectedRam.data(), expectedRam.size()) == 0, "scene/callback effects match; all other RAM and retired packet untouched");
            }
    std::printf("DC static program-chain span: %d original/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
