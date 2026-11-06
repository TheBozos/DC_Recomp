// Compare whole retail TexAnime with its exact guarded production adaptations.
// Packet helpers, Enable and float conversion execute their compiled originals;
// MGMoveImage is a controlled native-copy boundary, recording actual inputs.
#include "dc_functions.h"
#include "texanime_ref_functions.h"
#include "texanime_native_functions.h"
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
    constexpr uint32_t anime = 0x400000, entries = 0x410000, marker = 0x450000,
                       packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0;
    uint64_t copyCalls = 0, enableCalls = 0;
    std::vector<BozoFunctionEntry> packetFunctions;
    RecompiledFunction enableFunction = nullptr;
    struct Call
    {
        uint32_t pc = 0;
        std::array<uint32_t, 6> args{};
        std::array<uint32_t, 8> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    void half(uint8_t *ram, uint32_t at, int16_t v) { std::memcpy(ram + at, &v, 2); }
    void realPacket(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        ++packetCalls;
        for (const auto &entry : packetFunctions)
            if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
        throw std::runtime_error("unexpected TexAnime packet helper");
    }
    void enable(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        call.args[0] = g.arg(0); call.args[1] = g.arg(1);
        calls.push_back(call); ++enableCalls;
        enableFunction(ram, ctx, rt);
    }
    void copy(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        Call call{}; call.pc = ctx->pc;
        // Pointer identity for temporary stack records is immaterial; their
        // actual texture identifiers/rectangle bytes and scalar inputs are not.
        for (int i = 3; i < 6; ++i) call.args[i] = g.arg(i);
        std::memcpy(call.data.data(), g.in(g.arg(0), 8), 8);
        std::memcpy(call.data.data() + 2, g.in(g.arg(1), 16), 16);
        std::memcpy(call.data.data() + 6, g.in(g.arg(2), 8), 8);
        calls.push_back(call); ++copyCalls;
        g.write32(marker, g.read32(marker) + 1);
        g.ret(0); ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context(int group)
    {
        R5900Context ctx;
        for (int r = 16; r <= 23; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[4].uw[0] = anime; ctx.r[5].sd[0] = group;
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
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : {0x120c80u, 0x120e38u, 0x120ec0u, 0x121028u, 0x120ed8u, 0x120e90u})
    {
        packetFunctions.push_back({pc, rt->functions.generated(pc)});
        rt->functions.setOverride(pc, realPacket);
    }
    enableFunction = rt->functions.generated(0x167b40);
    rt->functions.setOverride(0x167b40, enable);
    rt->functions.setOverride(0x12f210, copy);
    auto *ram = rt->memory.ram();
    for (int profile = 0; profile < 6; ++profile)
        for (int edge = 0; edge < 4; ++edge)
            for (int active = 0; active < 3; ++active)
                for (bool paused : {false, true})
                    for (int groupMode = 0; groupMode < 3; ++groupMode)
                    {
                        ++cases;
                        const int group = groupMode == 2 ? -1 : 3;
                        std::memset(ram + anime, 0, 0x300);
                        std::memset(ram + entries, 0, 24 * 0x200);
                        for (uint32_t slot = 0; slot < 24; ++slot)
                        {
                            const uint32_t entry = entries + slot * 0x200;
                            const bool enabled = active == 2 || (active == 1 && (slot == 0 || slot == 7 || slot == 23));
                            word(ram, anime + slot * 4, enabled);
                            word(ram, anime + 0x60 + slot * 4, entry);
                            word(ram, anime + 0x120 + slot * 4, active == 2 && slot == 11 ? 0 : entry);
                            word(ram, anime + 0x180 + slot * 4, edge == 3 ? 0xffffffffu : edge);
                            for (uint32_t link = 0; link < 2; ++link)
                            {
                                const auto at = entry + link * 0x80;
                                const int kind = profile == 4 ? slot % 3 : profile == 2 ? 1 : profile == 3 ? 2 : 0;
                                half(ram, at, static_cast<int16_t>(kind));
                                half(ram, at + 4, profile == 5 && link == 0 ? 0 :
                                    static_cast<int16_t>(std::array<int, 3>{-1, 1, 3}[slot % 3]));
                                half(ram, at + 6, static_cast<int16_t>(slot % 4 == 0 ? (slot + 1) % 24 : -1));
                                half(ram, at + 8, static_cast<int16_t>(groupMode == 1 && slot % 3 == 0 ? 4 : group));
                                half(ram, at + 0x20, static_cast<int16_t>(groupMode == 1 && slot % 3 == 1 ? 4 : group));
                                half(ram, at + 0xa, 16); half(ram, at + 0xc, 12);
                                half(ram, at + 0xe, profile == 1 ? 1 : 0);
                                word(ram, at + 0x10, 0x00300000 + slot * 0x100 + link);
                                word(ram, at + 0x14, 0x12345678 + slot);
                                word(ram, at + 0x28, 0x00500000 + slot * 0x100 + link);
                                word(ram, at + 0x2c, 0x87654321 + slot);
                                half(ram, at + 0x38, static_cast<int16_t>(-5 + slot));
                                half(ram, at + 0x3a, static_cast<int16_t>(3 + slot));
                                half(ram, at + 0x3c, 16); half(ram, at + 0x3e, 12);
                                half(ram, at + 0x40, static_cast<int16_t>(20 - slot));
                                half(ram, at + 0x42, static_cast<int16_t>(30 + slot));
                                const float offset = std::array<float, 4>{-2.75f, 0.0f, 3.5f, 19.25f}[edge];
                                const float speed = std::array<float, 4>{-1.25f, 0.0f, 2.0f, 17.0f}[edge];
                                word(ram, at + 0x44, std::bit_cast<uint32_t>(speed));
                                word(ram, at + 0x48, std::bit_cast<uint32_t>(-speed));
                                word(ram, at + 0x4c, std::bit_cast<uint32_t>(offset));
                                word(ram, at + 0x50, std::bit_cast<uint32_t>(offset + 1.0f));
                                word(ram, at + 0x54, profile == 5 && link == 0 ? entry + 0x80 : 0);
                            }
                        }
                        word(ram, marker, 0);
                        word(ram, gp - 0x7170, paused);
                        word(ram, gp - 0x742c, packet);
                        std::memset(ram + packet, 0, 0x20000);
                        word(ram, packet, output); word(ram, packet + 4, output);
                        std::memset(ram + stack - 0x1000, 0xa5, 0x1000);
                        const std::vector<uint8_t> initialAnime(ram + anime, ram + anime + 0x300);
                        const std::vector<uint8_t> initialEntries(ram + entries, ram + entries + 24 * 0x200);
                        const std::vector<uint8_t> initialRam(ram, ram + bozo::kFlatScratchpadOffset);
                        auto original = context(group);
                        calls.clear(); packetCalls = 0;
                        texanime_ref_function_table[0].function(ram, &original, rt.get());
                        check(original.pc == 0x123450 && !original.leaving, "original returns normally");
                        check(packetCalls == 12, "original executes both six-call TEXFLUSH envelopes");
                        const auto expectedCalls = calls;
                        const std::vector<uint8_t> finalAnime(ram + anime, ram + anime + 0x300);
                        const std::vector<uint8_t> finalEntries(ram + entries, ram + entries + 24 * 0x200);
                        uint32_t finalMarker; std::memcpy(&finalMarker, ram + marker, 4);
                        auto expectedReference = initialRam;
                        std::memcpy(expectedReference.data() + anime, finalAnime.data(), finalAnime.size());
                        std::memcpy(expectedReference.data() + entries, finalEntries.data(), finalEntries.size());
                        std::memcpy(expectedReference.data() + marker, &finalMarker, 4);
                        std::memcpy(expectedReference.data() + packet, ram + packet, 0x20000);
                        check(sameNonStack(ram, expectedReference), "reference changes only logical state/copy marker and unused packets");
                        std::memcpy(ram + anime, initialAnime.data(), initialAnime.size());
                        std::memcpy(ram + entries, initialEntries.data(), initialEntries.size());
                        word(ram, marker, 0);
                        word(ram, gp - 0x742c, 0x12000000); // inaccessible packet pointer
                        std::memset(ram + packet, 0xa5, 0x20000);
                        std::memset(ram + stack - 0x1000, 0xa5, 0x1000);
                        std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                        std::memcpy(expectedRam.data() + anime, finalAnime.data(), finalAnime.size());
                        std::memcpy(expectedRam.data() + entries, finalEntries.data(), finalEntries.size());
                        std::memcpy(expectedRam.data() + marker, &finalMarker, 4);
                        auto native = context(group); const auto before = native;
                        calls.clear(); packetCalls = 0;
                        texanime_native_function_table[0].function(ram, &native, rt.get());
                        check(native.pc == 0x123450 && !native.leaving, "native returns normally");
                        check(packetCalls == 0 && calls == expectedCalls, "same ordered Enable/copy inputs without packet calls");
                        for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                            check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "saved full registers/stack/return preserved");
                        check(native.fcr31 == original.fcr31, "retained floating-point status matches");
                        check(sameNonStack(ram, expectedRam), "same animation state; packet and all other non-stack RAM untouched");
                    }
    check(copyCalls != 0 && enableCalls != 0, "fixtures reach native copies and compiled slot Enable");
    std::printf("DC TexAnime: %d original/adapted cases, %llu copy / %llu Enable calls, %d failures\n",
        cases, static_cast<unsigned long long>(copyCalls), static_cast<unsigned long long>(enableCalls), failures);
    return failures ? 1 : 0;
}
