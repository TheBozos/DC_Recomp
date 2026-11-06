// Original whole message-window function versus its two guarded adaptations.
// Rendering/texture helpers are controlled boundaries; text/layout arithmetic
// and message state transitions execute as statically compiled retail code.
#include "dc_functions.h"
#include "message_ref_functions.h"
#include "message_native_functions.h"
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
    constexpr uint32_t message = 0x400000, texture = 0x430000, glyph = 0x440000,
                       packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0;
    struct Call
    {
        uint32_t pc = 0;
        std::array<uint32_t, 11> args{};
        std::array<uint32_t, 8> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    std::vector<BozoFunctionEntry> packetFunctions;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void half(uint8_t *ram, uint32_t at, int16_t v) { std::memcpy(ram + at, &v, 2); }
    void packetCall(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        ++packetCalls;
        for (const auto &entry : packetFunctions)
            if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
        throw std::runtime_error("unexpected message packet call");
    }
    void callback(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        int count = 0, retiredArg = -1;
        switch (ctx->pc)
        {
        case 0x133070: count = 3; retiredArg = 1; break;
        case 0x151ad0: count = 2; retiredArg = 1; break;
        case 0x1312d0: count = 3; break;
        case 0x15bc10: count = 1; break;
        case 0x152030: count = 5; break;
        case 0x152cb0: count = 9; break;
        case 0x152930: count = 8; break;
        case 0x14e380: count = 1; break;
        case 0x14c220: case 0x14c4f0: count = 2; retiredArg = 0; break;
        case 0x14c5a0: count = 3; retiredArg = 0; break;
        case 0x150810: count = 11; retiredArg = 1; break;
        case 0x152ee0: count = 7; break;
        case 0x15c310: count = 5; retiredArg = 0; break;
        default: throw std::runtime_error("unexpected message rendering fixture callback");
        }
        for (int i = 0; i < count; ++i) if (i != retiredArg) call.args[i] = g.arg(i);
        if (ctx->pc == 0x152ee0 || ctx->pc == 0x15c310)
        {
            const int first = ctx->pc == 0x152ee0 ? 3 : 2;
            std::memcpy(call.data.data(), g.in(g.arg(first), 16), 16);
            std::memcpy(call.data.data() + 4, g.in(g.arg(first + 1), 16), 16);
        }
        if (ctx->pc == 0x152cb0)
        {
            g.write32(g.arg(7), g.read32(g.arg(7)) + 7);
            g.write32(g.arg(8), g.read32(g.arg(8)) - 5);
        }
        // A logical creation effect verifies it remains before texture selection.
        if (ctx->pc == 0x14e380) g.write32(message + 0x1800, 0xabcdef01);
        if (ctx->pc == 0x1312d0) g.retU(texture); else g.ret(0);
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
        ctx.r[4].uw[0] = message; ctx.r[28].uw[0] = gp;
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
    for (uint32_t pc : {0x133070u, 0x151ad0u, 0x1312d0u, 0x15bc10u, 0x152030u, 0x152cb0u,
         0x152930u, 0x14e380u, 0x14c220u, 0x14c5a0u, 0x150810u, 0x152ee0u, 0x14c4f0u, 0x15c310u})
        rt->functions.setOverride(pc, callback);
    auto *ram = rt->memory.ram();
    for (uint32_t mode = 0; mode < 8; ++mode)
        for (uint32_t pulse : {0u, 27u, 28u, 0xffffffffu})
            for (uint32_t highlighted : {0xffffffffu, 0u, 1u})
                for (bool suppressed : {false, true})
                {
                    ++cases;
                    std::memset(ram + message, 0, 0x1900);
                    word(ram, message, 60); word(ram, message + 4, 80);
                    word(ram, message + 8, mode & 1 ? 16 : 10); word(ram, message + 12, 20);
                    word(ram, message + 0x14, 4); word(ram, message + 0x18, 3); word(ram, message + 0x1c, 2);
                    word(ram, message + 0x20, 5); word(ram, message + 0x34, mode & 1);
                    word(ram, message + 0x54, mode & 2); word(ram, message + 0x94, mode == 7);
                    word(ram, message + 0x90, std::bit_cast<uint32_t>(mode == 7 ? 0.5f : 1.0f));
                    word(ram, message + 0x98, mode & 2); word(ram, message + 0x9c, mode & 4 ? 160 : 0);
                    word(ram, message + 0xa0, mode & 4 ? 80 : 0); word(ram, message + 0xac, 1);
                    word(ram, message + 0xb8, 5); word(ram, message + 0xbc, 0);
                    word(ram, message + 0x16bc, mode == 6 ? 0xffffffffu : 0);
                    word(ram, message + 0x16b4, pulse); word(ram, message + 0x16cc, mode & 4);
                    word(ram, message + 0x16c4, 1); word(ram, message + 0x16c8, mode & 2);
                    word(ram, message + 0x16d4, mode & 4 ? 200 : 90);
                    word(ram, message + 0x16d8, mode % 3); word(ram, message + 0x16dc, mode & 2);
                    word(ram, message + 0x1740, highlighted); word(ram, message + 0x1744, 11);
                    word(ram, message + 0x1748, mode & 2 ? 1 : 0); word(ram, message + 0x174c, 7);
                    word(ram, message + 0x17b4, mode == 5 ? 0 : glyph);
                    for (int i = 0; i < 5; ++i)
                    {
                        const int16_t code = std::array<int16_t, 5>{-800, -768, -735, -612, -611}[i];
                        half(ram, message + 0xc4 + i * 8, code);
                        half(ram, message + 0xc6 + i * 8, static_cast<int16_t>(i * 10));
                        half(ram, message + 0xc8 + i * 8, static_cast<int16_t>(i * 20));
                        word(ram, message + 0x1750 + i * 8, mode & 4 ? 0xffffffffu : 2);
                        word(ram, message + 0x1754 + i * 8, mode & 4 ? 0xffffffffu : 5);
                        if (code >= -768 && code < -611)
                        {
                            const auto at = 0x2529d0 + static_cast<uint32_t>(code + 768) * 32;
                            for (uint32_t f = 0; f < 8; ++f) word(ram, at + f * 4, f == 0 ? 0xfffffd43 : f * 3 + i);
                        }
                    }
                    word(ram, gp - 0x724c, suppressed);
                    word(ram, gp - 0x742c, packet);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output); word(ram, packet + 4, output);
                    const std::vector<uint8_t> initialMessage(ram + message, ram + message + 0x1900);
                    std::memset(ram + glyph, 0xa5, 0x200);
                    calls.clear(); packetCalls = 0;
                    auto original = context();
                    message_ref_function_table[0].function(ram, &original, rt.get());
                    check(original.pc == 0x123450 && !original.leaving, "original message draw returns normally");
                    const auto expectedCalls = calls;
                    const int expectedPackets = mode == 6 || mode == 7 ? 0 : 6;
                    check(packetCalls == expectedPackets, "reference emits exactly the audited flush envelope when reached");
                    const std::vector<uint8_t> finalMessage(ram + message, ram + message + 0x1900);
                    const std::vector<uint8_t> finalGlyph(ram + glyph, ram + glyph + 0x200);
                    std::memcpy(ram + message, initialMessage.data(), initialMessage.size());
                    std::memset(ram + glyph, 0xa5, 0x200);
                    word(ram, gp - 0x742c, 0x12000000);
                    std::memset(ram + packet, 0xa5, 0x20000);
                    std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                    std::memcpy(expectedRam.data() + message, finalMessage.data(), finalMessage.size());
                    std::memcpy(expectedRam.data() + glyph, finalGlyph.data(), finalGlyph.size());
                    auto native = context(); const auto before = native;
                    calls.clear(); packetCalls = 0;
                    message_native_function_table[0].function(ram, &native, rt.get());
                    check(native.pc == 0x123450 && !native.leaving, "adapted message draw returns normally");
                    check(calls == expectedCalls && packetCalls == 0, "same ordered texture/layout/glyph/native draw inputs without packet calls");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "saved full registers/stack/return preserved");
                    check(native.fcr31 == original.fcr31, "retained float status matches original");
                    check(std::memcmp(ram, expectedRam.data(), stack - 0x1000) == 0 &&
                          std::memcmp(ram + stack, expectedRam.data() + stack, expectedRam.size() - stack) == 0,
                          "message/glyph state matches; packet and all other non-stack RAM untouched");
                }
    std::printf("DC message wrapper: %d original/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
