// Whole retail native-copy owners before/after exact production adaptations.
// Packet builders and float conversion run compiled originals. Texture lookup
// and copy submission are controlled boundaries, comparing actual inputs.
#include "dc_functions.h"
#ifdef DC_ITEM_COPY
#include "item_state_ref_functions.h"
#include "item_state_native_functions.h"
#define REFERENCE item_state_ref_function_table
#define ADAPTED item_state_native_function_table
constexpr bool item = true;
#else
#include "texanim_state_ref_functions.h"
#include "texanim_state_native_functions.h"
#define REFERENCE texanim_state_ref_function_table
#define ADAPTED texanim_state_native_function_table
constexpr bool item = false;
#endif
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
    constexpr uint32_t textures = 0x400000, marker = 0x430000, packet = 0x500000,
        output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0, packetCalls = 0, textureRequests = 0;
    uint32_t presence = 0;
    bool sameTexture = false;
    uint64_t copyCalls = 0;
    std::vector<BozoFunctionEntry> packetFunctions;
    struct Call
    {
        uint32_t pc = 0;
        std::array<uint32_t, 16> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
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
        throw std::runtime_error("unexpected copy wrapper packet helper");
    }
    void boundary(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx); Call call{}; call.pc = ctx->pc;
        if (ctx->pc == 0x1312d0)
        {
            for (int i = 0; i < 3; ++i) call.data[i] = g.arg(i);
            const uint32_t index = textureRequests++;
            g.retU((presence & (1u << index)) ? textures + (sameTexture ? 0 : index * 0x100) : 0);
        }
        else if (ctx->pc == 0x12f210)
        {
            // Native MGMoveImage consumes texture identifiers, rectangle and
            // destination coordinates, never console packet bytes.
            std::memcpy(call.data.data(), g.in(g.arg(0), 8), 8);
            std::memcpy(call.data.data() + 2, g.in(g.arg(1), 16), 16);
            std::memcpy(call.data.data() + 6, g.in(g.arg(2), 8), 8);
            for (int i = 3; i < 6; ++i) call.data[8 + i - 3] = g.arg(i);
            ++copyCalls; g.write32(marker, g.read32(marker) + 1); g.ret(0);
        }
        else if (ctx->pc == 0x140660)
        {
            // Native MoveImageTest ignores a0 (packet); arguments 8..10 are
            // real stack arguments and must survive envelope retirement too.
            for (int i : {1,2,3,5,6,7,8,9,10}) call.data[i] = g.arg(i);
            std::memcpy(call.data.data() + 11, g.in(g.arg(4), 16), 16);
            ++copyCalls; g.write32(marker, g.read32(marker) + 1); g.ret(0);
        }
        else throw std::runtime_error("unexpected copy wrapper boundary");
        calls.push_back(call); ctx->pc = ctx->r[31].uw[0];
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0 program"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(int edge)
    {
        R5900Context ctx;
        for (int r = 16; r <= 23; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[4].uw[0] = 0x440000; ctx.r[7].uw[0] = 0x440100;
        for (int r : {5,6,8,9}) ctx.r[r].sd[0] = edge == 0 ? 0 : edge == 1 ? -1 : edge == 2 ? 31 : 0x7fffffff;
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
    for (uint32_t pc : {0x120c80u, 0x120e38u, 0x120ec0u, 0x121028u, 0x120ed8u, 0x120e90u})
    {
        packetFunctions.push_back({pc, rt.functions.generated(pc)}); rt.functions.setOverride(pc, packetCall);
    }
    rt.functions.setOverride(0x1312d0, boundary);
    rt.functions.setOverride(item ? 0x140660 : 0x12f210, boundary);
    auto *ram = rt.memory.ram();
    for (presence = 0; presence < 4; ++presence)
        for (int phase = 0; phase < (item ? 4 : 6); ++phase)
            for (int step = 0; step < 3; ++step)
                for (int alias = 0; alias < 2; ++alias)
                {
                    ++cases;
                    sameTexture = alias != 0;
                    const float phases[] = {-1.0f, 0.0f, 6.0f, 6.999f, 7.0f, 8.0f};
                    const float steps[] = {-1.0f, 0.0f, 0.25f};
                    word(ram, gp - 0x6cf0, std::bit_cast<uint32_t>(phases[phase]));
                    word(ram, gp - 0x7d64, std::bit_cast<uint32_t>(steps[step]));
                    word(ram, gp - 0x6cf4, 0xdeadbeef);
                    // Include arbitrary high TEX0 bits and unequal widths/bases;
                    // the item wrapper must retain its exact original decoding.
                    for (int index = 0; index < 2; ++index)
                    {
                        const uint64_t value = 0xabcd123456780000ull + index * 0x4241 + step * 0x110;
                        std::memcpy(ram + textures + index * 0x100 + 0x28, &value, 8);
                    }
                    // Original item code has no absent-texture guard: preserve
                    // its RAM-zero-page input, without inventing a fallback.
                    const uint64_t zeroRecord = 0x9876543212345678ull;
                    std::memcpy(ram + 0x28, &zeroRecord, 8);
                    word(ram, marker, 0); word(ram, gp - 0x742c, packet);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output); word(ram, packet + 4, output);
                    std::memset(ram + stack - 0x1000, 0xa5, 0x1000);
                    const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
                    auto reference = context(phase); if (alias) reference.r[7] = reference.r[4];
                    calls.clear(); packetCalls = textureRequests = 0;
                    REFERENCE[0].function(ram, &reference, &rt);
                    check(reference.pc == 0x123450 && !reference.leaving, "original returns normally");
                    check(textureRequests == 2, "both lookups retained including absent textures");
                    check(packetCalls == (item || presence == 3 ? 12 : 6), "exact conditional original flush envelopes");
                    const auto expectedCalls = calls;
                    auto expectedReference = initial;
                    std::memcpy(expectedReference.data() + gp - 0x6cf4, ram + gp - 0x6cf4, 8);
                    std::memcpy(expectedReference.data() + marker, ram + marker, 4);
                    std::memcpy(expectedReference.data() + packet, ram + packet, 0x20000);
                    check(sameNonStack(ram, expectedReference), "reference writes only phase/copy marker and unused packets");
                    std::memcpy(ram, initial.data(), initial.size());
                    word(ram, gp - 0x742c, 0x12000000); std::memset(ram + packet, 0xa5, 0x20000);
                    auto expectedNative = expectedReference;
                    word(expectedNative.data(), gp - 0x742c, 0x12000000);
                    std::memset(expectedNative.data() + packet, 0xa5, 0x20000);
                    auto native = context(phase); if (alias) native.r[7] = native.r[4]; const auto before = native;
                    calls.clear(); packetCalls = textureRequests = 0;
                    ADAPTED[0].function(ram, &native, &rt);
                    check(native.pc == 0x123450 && !native.leaving, "adapted returns normally");
                    check(packetCalls == 0 && calls == expectedCalls, "same ordered lookups/copy inputs without packet calls");
                    check(sameNonStack(ram, expectedNative), "same logical phase/copy effects, no other non-stack or packet writes");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "full saved registers/stack/return preserved");
                    check(native.fcr31 == reference.fcr31, "retained float status matches");
                }
    check(copyCalls != 0, "fixtures reach actual copy boundary");
    std::printf("DC %s: %d original/adapted cases, %llu copy calls, %d failures\n", item ? "item copy" : "texture animation copy",
        cases, static_cast<unsigned long long>(copyCalls), failures);
    return failures ? 1 : 0;
}
