// Original/adapted overlay owners. Real compiled packet builders, RNG and float
// conversion; controlled native texture/camera boundaries compare used inputs.
#include "dc_functions.h"
#include "overlay_case.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace
{
constexpr uint32_t textures = 0x440000, seed = 0x490000, marker = 0x480000,
    packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
int failures = 0, cases = 0, packetCalls = 0, textureRequests = 0, presence = 0;
uint64_t copyCalls = 0, semanticCalls = 0, rngCalls = 0;
bool alias = false;
std::vector<BozoFunctionEntry> originals;
struct Call
{
    uint32_t pc = 0;
    std::array<uint32_t, 16> data{};
    bool operator==(const Call &) const = default;
};
std::vector<Call> calls;
void check(bool ok, const char *what)
{
    if (!ok) { ++failures; std::printf("FAIL %x case %d: %s\n", owner, cases, what); }
}
void word(uint8_t *ram, uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); }
void number(uint8_t *ram, uint32_t at, float value) { word(ram, at, std::bit_cast<uint32_t>(value)); }
bool packetHelper(uint32_t pc)
{
    for (uint32_t p : {0x120c28u,0x120c80u,0x120d70u,0x120e38u,0x120ec0u,0x121028u,0x120ed8u,0x120e90u})
        if (p == pc) return true;
    return false;
}
void originalCall(uint8_t *ram, R5900Context *ctx, Runtime *rt)
{
    if (packetHelper(ctx->pc)) ++packetCalls;
    if (ctx->pc == 0x1046f8) ++rngCalls;
    for (const auto &entry : originals)
        if (entry.address == ctx->pc) { entry.function(ram, ctx, rt); return; }
    throw std::runtime_error("unexpected original overlay helper");
}
void boundary(uint8_t *, R5900Context *ctx, Runtime *rt)
{
    bozo::libs::Guest g(*rt, ctx); Call call{}; call.pc = ctx->pc;
    if (ctx->pc == 0x1312d0)
    {
        for (int i = 0; i < 3; ++i) call.data[i] = g.arg(i);
        const uint32_t index = textureRequests++;
        if (index > 1) throw std::runtime_error("unexpected third texture lookup");
        g.retU((presence & (1u << index)) ? textures + (alias ? 0 : index * 0x100) : 0);
    }
    else if (ctx->pc == 0x12f210)
    {
        std::memcpy(call.data.data(), g.in(g.arg(0), 8), 8);
        std::memcpy(call.data.data() + 2, g.in(g.arg(1), 16), 16);
        std::memcpy(call.data.data() + 6, g.in(g.arg(2), 8), 8);
        for (int i = 3; i < 6; ++i) call.data[8 + i - 3] = g.arg(i);
        ++copyCalls; g.ret(0);
    }
    else if (ctx->pc == 0x140660)
    {
        // a0 is the obsolete packet argument. The native copy consumes all
        // other inputs, including arguments 8..10 on the caller's stack.
        for (int i : {1,2,3,5,6,7,8,9,10}) call.data[i] = g.arg(i);
        std::memcpy(call.data.data() + 11, g.in(g.arg(4), 16), 16);
        ++copyCalls; g.ret(0);
    }
    else
    {
        switch (ctx->pc)
        {
        case 0x12df70:
            call.data[0] = g.arg(0);
            std::memcpy(call.data.data() + 1, g.in(g.arg(0), 16), 16); break;
        case 0x12dfa0:
        case 0x124670:
            call.data[0] = g.arg(0); call.data[1] = g.arg(1);
            std::memcpy(call.data.data() + 2, g.in(g.arg(1), 16), 16); break;
        case 0x1247d0:
            call.data[0] = g.arg(0); call.data[1] = g.arg(1); break;
        case 0x1596f0:
        case 0x13df40:
            call.data[0] = g.arg(0); break;
        case 0x133070:
            // ReloadTexture ignores its compatibility packet argument a1.
            call.data[0] = g.arg(0); call.data[2] = g.arg(2); break;
        default: throw std::runtime_error("unexpected native overlay boundary");
        }
        g.ret(0);
    }
    ++semanticCalls;
    g.write32(marker, g.read32(marker) * 33 + call.pc);
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
R5900Context context(int selector)
{
    R5900Context ctx;
    for (int r = 16; r <= 23; ++r)
    {
        ctx.r[r].ud[0] = 0x1234567800000000ull + r;
        ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
    }
    ctx.r[4].uw[0] = selector;
    ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
    return ctx;
}
bool sameNonStack(const uint8_t *ram, const std::vector<uint8_t> &expected)
{
    return std::memcmp(ram, expected.data(), stack - 0x1000) == 0 &&
        std::memcmp(ram + stack, expected.data() + stack, expected.size() - stack) == 0;
}
void loadOverlay(uint8_t *ram, const char *path)
{
    std::ifstream file(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    auto u32 = [&](size_t at) { uint32_t v; if (at + 4 > bytes.size()) throw std::runtime_error("bad overlay ELF"); std::memcpy(&v, bytes.data() + at, 4); return v; };
    if (u32(0) != 0x464c457f) throw std::runtime_error("missing overlay ELF");
    for (uint32_t i = 0; i < (u32(44) & 0xffff); ++i)
    {
        const auto at = u32(28) + i * 32;
        if (u32(at) != 1) continue;
        const auto offset = u32(at + 4), address = u32(at + 8), size = u32(at + 16), memorySize = u32(at + 20);
        if (size > memorySize || offset + uint64_t(size) > bytes.size() || address + uint64_t(memorySize) > bozo::kFlatScratchpadOffset)
            throw std::runtime_error("bad overlay segment");
        std::memset(ram + address, 0, memorySize); std::memcpy(ram + address, bytes.data() + offset, size);
    }
}
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    Host host; bozo::ee::Dispatcher dispatcher(host); auto &rt = dispatcher.runtime();
    rt.functions.setGenerated(dc_function_table, dc_function_count);
    for (uint32_t pc : helpers)
    {
        if (packetHelper(pc) || pc == 0x1046f8 || pc == 0x1110b0)
        {
            auto original = rt.functions.generated(pc);
            if (!original) throw std::runtime_error("missing original resident helper");
            originals.push_back({pc, original}); rt.functions.setOverride(pc, originalCall);
        }
        else rt.functions.setOverride(pc, boundary);
    }
    auto *ram = rt.memory.ram(); loadOverlay(ram, argv[1]);
    const std::vector<uint8_t> overlay(ram, ram + bozo::kFlatScratchpadOffset);
    const bool face = owner == 0x1db87d0 || owner == 0x1dbba90 || owner == 0x1dc3c90 || owner == 0x1dc8930;
    for (presence = 0; presence < 4; ++presence)
        for (int phase = 0; phase < (isolatedSpan ? 2 : 6); ++phase)
            for (int selector = 0; selector < (owner == 0x1db87d0 ? 8 : face ? 3 : 1); ++selector)
                for (int mode = 0; mode < (face ? 4 : 2); ++mode)
                {
                    ++cases; alias = (mode & 1) != 0;
                    std::memcpy(ram, overlay.data(), overlay.size());
                    const float phases[] = {-1.f,0.f,0.5f,3.5f,7.f,127.f};
                    for (uint32_t offset : phaseOffsets) number(ram, gp + offset, phases[phase]);
                    for (uint32_t offset : flagOffsets) word(ram, gp + offset, mode & 1);
                    word(ram, gp - 0x67f8, phase == 5 ? 1 : 0);
                    if (face && mode >= 2)
                    {
                        // Some retail face tables are populated by scene setup.
                        // Keep raw/empty-table profiles too; these synthetic
                        // records exercise both copies and writable bookkeeping.
                        const uint32_t table = owner == 0x1db87d0 ? 0x1dd67f0 : owner == 0x1dbba90 ? 0x1dd77c0 :
                            owner == 0x1dc3c90 ? 0x1dda9c0 : 0x1ddb0f0;
                        const uint32_t row = table + (owner == 0x1dc8930 ? 0 : selector * 44);
                        const uint32_t values[] = {0x460000u + selector * 16u,0x470000u + selector * 16u,2,8,3,4,0,0,128,16,32};
                        std::memcpy(ram + row, values, sizeof(values));
                    }
                    for (int i = 0; i < 8; ++i)
                    {
                        const uint32_t state = 0x1de1b44 + i * 52;
                        word(ram, state, phase % 3); word(ram, state + 8, phase % 3);
                        number(ram, state + 12, phases[phase]); word(ram, state + 16, mode & 1);
                    }
                    word(ram, 0x24fdec, seed); word(ram, seed + 0x58, 0xdeadbeef + phase);
                    for (int i = 0; i < 2; ++i)
                    {
                        const uint64_t tex0 = 0xabcd123456780000ull + i * 0x4241 + phase * 0x110;
                        std::memcpy(ram + textures + i * 0x100 + 0x28, &tex0, 8);
                    }
                    word(ram, marker, 0); word(ram, gp - 0x742c, packet);
                    std::memset(ram + packet, 0, 0x20000); word(ram, packet, output); word(ram, packet + 4, output);
                    std::memset(ram + stack - 0x1000, 0xa5, 0x1200);
                    const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
                    auto reference = context(selector);
                    calls.clear(); packetCalls = textureRequests = 0;
                    REFERENCE[0].function(ram, &reference, &rt);
                    check(reference.pc == 0x123450 && !reference.leaving, "original returns normally");
                    check(packetCalls >= 1, "original reaches packet helpers");
                    const auto expectedCalls = calls;
                    std::vector<uint8_t> expected(ram, ram + bozo::kFlatScratchpadOffset);
                    std::memcpy(ram, initial.data(), initial.size());
                    word(ram, gp - 0x742c, 0x12000000); std::memset(ram + packet, 0xa5, 0x20000);
                    word(expected.data(), gp - 0x742c, 0x12000000); std::memset(expected.data() + packet, 0xa5, 0x20000);
                    auto native = context(selector); const auto before = native;
                    calls.clear(); packetCalls = textureRequests = 0;
                    ADAPTED[0].function(ram, &native, &rt);
                    check(native.pc == 0x123450 && !native.leaving, "adapted returns normally");
                    check(packetCalls == 0 && calls == expectedCalls, "same ordered semantic calls without packets");
                    check(sameNonStack(ram, expected), "same non-stack effects and untouched packet guards");
                    for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                        if (!(isolatedSpan && r == 23))
                            check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "saved registers preserved");
                    check(native.fcr31 == reference.fcr31, "float status retained");
                }
    check(semanticCalls != 0, "reached native semantic boundaries");
    if (!isolatedSpan) check(copyCalls != 0, "reached texture copy boundary");
    if (face && owner != 0x1dc8930) check(rngCalls != 0, "reached real compiled RNG");
    std::printf("DC overlay %x: %d original/adapted cases, %llu copy calls, %llu RNG calls, %d failures\n", owner, cases,
        static_cast<unsigned long long>(copyCalls), static_cast<unsigned long long>(rngCalls), failures);
    return failures ? 1 : 0;
}
