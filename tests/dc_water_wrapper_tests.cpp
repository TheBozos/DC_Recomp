// Same locally extracted wrapper before/after the production packet adaptations.
#include "dc_functions.h"
#include "water_ref_functions.h"
#include "water_native_functions.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t water = 0x400000, renderInfo = 0x410000, vtable = 0x420000;
    constexpr uint32_t packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0;
    uint32_t clipResult = 0, drawResult = 0;
    struct Call
    {
        uint32_t pc;
        std::array<uint32_t, 8> args;
        std::array<uint32_t, 16> matrix{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    std::vector<BozoFunctionEntry> packetFunctions;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    uint32_t word(uint8_t *ram, uint32_t at) { uint32_t v; std::memcpy(&v, ram + at, 4); return v; }
    void packetCall(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        ++packetCalls;
        for (const auto &entry : packetFunctions)
            if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
        throw std::runtime_error("unexpected packet fixture call");
    }
    void callback(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest guest(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        for (int i = 0; i < 8; ++i) call.args[i] = guest.arg(i);
        // Only compare live arguments for each helper; other caller-volatile
        // registers may differ when an obsolete packet helper is removed.
        if (ctx->pc == 0x161100)
        {
            for (int i = 1; i < 8; ++i) call.args[i] = 0;
            guest.retU(clipResult);
        }
        else if (ctx->pc == 0x1281b0)
        {
            for (int i = 2; i < 8; ++i) call.args[i] = 0;
            for (uint32_t i = 0; i < 16; ++i) guest.write32(guest.arg(1) + i * 4, 0x3f800000u + i * 397u);
            guest.ret(0);
        }
        else if (ctx->pc == 0x1607e0)
        {
            for (int i = 3; i < 8; ++i) call.args[i] = 0;
            word(ram, water + 0x88, guest.arg(1)); word(ram, water + 0x8c, 0);
            guest.ret(0);
        }
        else
        {
            // The native virtual draw ignores only its packet argument.
            call.args[1] = 0;
            std::memcpy(call.matrix.data(), guest.in(guest.arg(2), 64), 64);
            check(word(ram, renderInfo + 0x324) == 0, "native draw sees the retained RenderInfo update");
            guest.retU(drawResult);
        }
        calls.push_back(call);
        ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context(uint32_t pkt)
    {
        R5900Context ctx;
        for (int r = 16; r <= 23; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[4].uw[0] = water; ctx.r[5].uw[0] = renderInfo; ctx.r[6].uw[0] = pkt;
        ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x120c80u, 0x120e38u, 0x120ec0u, 0x121028u, 0x120ed8u, 0x120e90u})
    {
        packetFunctions.push_back({pc, rt->functions.generated(pc)});
        rt->functions.setOverride(pc, packetCall);
    }
    for (uint32_t pc : {0x161100u, 0x1281b0u, 0x1607e0u, 0x134bc0u, 0x135000u}) rt->functions.setOverride(pc, callback);
    auto *ram = rt->memory.ram();
    for (uint32_t clipped : {0u, 1u, 0xffffffffu})
        for (uint32_t frame : {0u, 1u, 2u, 0xffffffffu})
            for (uint32_t result : {0u, 1u, 64u, 0xffffffffu})
                for (uint32_t target : {0x134bc0u, 0x135000u})
                {
                    ++cases; clipResult = clipped; drawResult = result;
                    std::memset(ram + water, 0x6b, 0x100);
                    std::memset(ram + renderInfo, 0x5a, 0x400);
                    word(ram, water + 0x64, 0x620000); word(ram, water + 0x68, 0x630000);
                    word(ram, water + 0x78, vtable); word(ram, vtable + 0x1c, target);
                    word(ram, gp - 0x7440, frame);
                    const std::vector<uint8_t> initialWater(ram + water, ram + water + 0x100);
                    const std::vector<uint8_t> initialInfo(ram + renderInfo, ram + renderInfo + 0x400);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output); word(ram, packet + 4, output);
                    packetCalls = 0; calls.clear();
                    auto original = context(packet);
                    water_ref_function_table[0].function(ram, &original, rt.get());
                    check(original.pc == 0x123450 && !original.leaving, "original wrapper returns normally");
                    check(packetCalls == (clipped ? 0 : 12), "original emits only the two audited packet envelopes");
                    const auto expectedCalls = calls;
                    const std::vector<uint8_t> expectedWater(ram + water, ram + water + 0x100);
                    const std::vector<uint8_t> expectedInfo(ram + renderInfo, ram + renderInfo + 0x400);
                    std::memcpy(ram + water, initialWater.data(), initialWater.size());
                    std::memcpy(ram + renderInfo, initialInfo.data(), initialInfo.size());
                    std::memset(ram + packet, 0xa5, 0x20000);
                    std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                    std::memcpy(expectedRam.data() + water, expectedWater.data(), expectedWater.size());
                    std::memcpy(expectedRam.data() + renderInfo, expectedInfo.data(), expectedInfo.size());
                    packetCalls = 0; calls.clear();
                    auto native = context(0x12000000); const auto before = native;
                    water_native_function_table[0].function(ram, &native, rt.get());
                    check(native.pc == 0x123450 && !native.leaving && native.r[2].ud[0] == original.r[2].ud[0],
                          "clipping and full draw return value match original");
                    check(calls == expectedCalls, "retained helper order, live arguments and matrix match original");
                    check(packetCalls == 0, "adapted wrapper never calls a packet builder");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0,
                              "callee-saved registers, stack and return remain intact");
                    check(std::memcmp(ram, expectedRam.data(), stack - 0x1000) == 0 &&
                          std::memcmp(ram + stack, expectedRam.data() + stack, expectedRam.size() - stack) == 0,
                          "logical records match; all RAM outside original ABI stack scratch stays unchanged");
                }
    std::printf("DC water wrapper: %d original/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
