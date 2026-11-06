// Compare the same local retail wrapper before/after production packet retirement.
#include "dc_functions.h"
#include "effect_ref_functions.h"
#include "effect_native_functions.h"
#include "bozo/libs/guest.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t effect = 0x400000, packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0, textureRequest = 0;
    uint32_t textureMask = 0, projectResult = 0;
    std::array<float, 4> color{};
    struct Call
    {
        uint32_t pc;
        std::array<uint32_t, 4> args{};
        std::array<uint32_t, 24> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    std::vector<BozoFunctionEntry> packetFunctions;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void packetCall(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        ++packetCalls;
        for (const auto &entry : packetFunctions)
            if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
        throw std::runtime_error("unexpected effect packet fixture call");
    }
    void callback(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest guest(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        if (ctx->pc == 0x1312d0)
        {
            for (int i = 0; i < 3; ++i) call.args[i] = guest.arg(i);
            const auto index = textureRequest++;
            guest.retU((textureMask & (1u << index)) ? 0x430000 + index * 0x1000 : 0);
        }
        else if (ctx->pc == 0x12e990)
        {
            call.args[0] = guest.arg(0); call.args[1] = guest.arg(1);
            std::memcpy(call.data.data(), guest.in(guest.arg(1), 16), 16);
            guest.write(guest.arg(0), color.data(), 16); guest.ret(0);
        }
        else if (ctx->pc == 0x1110b0)
        {
            call.data[0] = guest.floatArg(0);
            guest.ret(static_cast<int32_t>(std::bit_cast<float>(guest.floatArg(0))));
        }
        else if (ctx->pc == 0x12e740)
        {
            for (int i = 0; i < 4; ++i) call.args[i] = guest.arg(i);
            std::memcpy(call.data.data(), guest.in(guest.arg(2), 16), 16);
            call.data[4] = guest.floatArg(0); call.data[5] = guest.floatArg(1);
            for (uint32_t i = 0; i < 4; ++i)
            {
                guest.write32(guest.arg(0) + i * 4, call.data[i] + 100 + i);
                guest.write32(guest.arg(1) + i * 4, call.data[i] + 200 + i);
            }
            guest.retU(projectResult);
        }
        else if (ctx->pc == 0x15d4b0)
        {
            // The native sprite reads texture, source rectangle, corners and color,
            // never its packet argument. Capture the actual submitted data.
            call.args[0] = guest.arg(1);
            std::memcpy(call.data.data(), guest.in(guest.arg(2), 16), 16);
            for (int i = 0; i < 4; ++i)
                std::memcpy(call.data.data() + 4 + i * 4, guest.in(guest.arg(3 + i), 16), 16);
            call.data[20] = guest.read32(guest.arg(7));
            guest.ret(0);
        }
        else throw std::runtime_error("unexpected effect fixture callback");
        calls.push_back(call); ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context()
    {
        R5900Context ctx;
        for (int r = 16; r <= 23; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[4].uw[0] = effect; ctx.r[28].uw[0] = gp;
        ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
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
    for (uint32_t pc : {0x1312d0u, 0x12e990u, 0x1110b0u, 0x12e740u, 0x15d4b0u}) rt->functions.setOverride(pc, callback);
    auto *ram = rt->memory.ram();
    for (textureMask = 0; textureMask < 4; ++textureMask)
        for (uint32_t pulse : {0u, 1u, 2u, 0xffffffffu})
            for (uint32_t projected : {0u, 1u, 0xffffffffu})
                for (uint32_t pattern = 0; pattern < 4; ++pattern)
                    for (uint32_t colored : {0u, 1u})
                    {
                        ++cases; projectResult = projected;
                        color = {pattern * 100.0f - 100.0f, pattern * 120.0f, pattern * 140.0f - 50.0f, 128};
                        std::memset(ram + effect, 0, 0x100);
                        word(ram, effect, colored);
                        for (uint32_t slot = 0; slot < 8; ++slot)
                        {
                            const uint32_t life = pattern == 0 ? 0 : pattern == 1 ? (slot == 3 ? 1 : 0) :
                                                  pattern == 2 ? slot + 1 : slot % 2 ? 20 : 0;
                            word(ram, effect + 0xb0 + slot * 4, life);
                            for (uint32_t c = 0; c < 4; ++c)
                                word(ram, effect + 0x10 + slot * 16 + c * 4, 0x3f800000 + slot * 100 + c);
                        }
                        word(ram, gp - 0x7188, pulse); word(ram, gp - 0x742c, packet);
                        std::memset(ram + packet, 0, 0x20000);
                        word(ram, packet, output); word(ram, packet + 4, output);
                        const std::vector<uint8_t> initialEffect(ram + effect, ram + effect + 0x100);
                        textureRequest = packetCalls = 0; calls.clear();
                        auto original = context();
                        effect_ref_function_table[0].function(ram, &original, rt.get());
                        check(original.pc == 0x123450 && !original.leaving, "original effect returns normally");
                        check(packetCalls == (textureMask == 3 ? 12 : 0), "reference emits exactly the audited packet calls");
                        const auto expectedCalls = calls;
                        uint32_t expectedPulse; std::memcpy(&expectedPulse, ram + gp - 0x7188, 4);
                        const std::vector<uint8_t> expectedEffect(ram + effect, ram + effect + 0x100);
                        word(ram, gp - 0x7188, pulse); word(ram, gp - 0x742c, 0x12000000);
                        std::memcpy(ram + effect, initialEffect.data(), initialEffect.size());
                        std::memset(ram + packet, 0xa5, 0x20000);
                        std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                        word(expectedRam.data(), gp - 0x7188, expectedPulse);
                        std::memcpy(expectedRam.data() + effect, expectedEffect.data(), expectedEffect.size());
                        textureRequest = packetCalls = 0; calls.clear();
                        auto native = context(); const auto before = native;
                        effect_native_function_table[0].function(ram, &native, rt.get());
                        check(native.pc == 0x123450 && !native.leaving, "adapted effect returns normally");
                        check(calls == expectedCalls, "texture/color/projection callbacks and native sprite inputs match original");
                        check(packetCalls == 0, "adapted effect never calls a packet builder");
                        for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                            check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0,
                                  "callee-saved registers, stack and return remain intact");
                        check(native.fcr31 == original.fcr31, "retained float calculations preserve original status");
                        check(std::memcmp(ram, expectedRam.data(), stack - 0x1000) == 0 &&
                              std::memcmp(ram + stack, expectedRam.data() + stack, expectedRam.size() - stack) == 0,
                              "alternating state/effect data match; no packet or other non-stack writes");
                    }
    std::printf("DC effect wrapper: %d original/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
