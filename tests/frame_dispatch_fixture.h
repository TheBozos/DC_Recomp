// Controlled virtual methods isolate the outer packet wrapper. The live route
// separately exercises the retained compiled hierarchy/light implementation.
#include "dc_functions.h"
#include "bozo/port.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    constexpr uint32_t frame = 0x400000, vtable = 0x401000, packet = 0x500000,
                       output = 0x510000, gp = 0x300000, stack = 0x700000, caller = 0x123450;
    int failures = 0, cases = 0, packetCalls = 0, drawCalls = 0;
    uint32_t drawResult = 0;
    std::array<uint32_t, 3> drawArgs{};
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
        throw std::runtime_error("unexpected frame packet call");
    }
    void draw(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        ++drawCalls;
        bozo::libs::Guest g(*rt, ctx);
        for (int i = 0; i < 3; ++i) drawArgs[i] = g.arg(i);
        // Stand-in logical side effect; the output pointer is ignored by native draws.
        g.write32(g.arg(0) + 0x100, g.read32(g.arg(2)) ^ 0x12345678u);
        g.retU(drawResult); ctx->pc = ctx->r[31].uw[0];
    }
    R5900Context context(uint32_t object = frame)
    {
        R5900Context ctx;
        for (int r = 16; r <= 31; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[2].ud[0] = 0x6789; ctx.r[4].uw[0] = object;
        ctx.r[5].uw[0] = packet; ctx.r[6].uw[0] = 0x402000;
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = caller;
        ctx.pc = wrapperPc;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    bozo::port::Registry registry;
    dc::native::install(*rt, registry);
    const auto nativeDraw = registry.find(wrapperPc);
    check(nativeDraw != nullptr, "production frame wrapper installed");
    if (!nativeDraw) return 1;
    for (uint32_t pc : {0x120c28u, 0x120f78u})
    {
        packetFunctions.push_back({pc, rt->functions.generated(pc)});
        rt->functions.setOverride(pc, packetCall);
    }
    for (uint32_t pc : {0x129400u, 0x12a1a0u}) rt->functions.setOverride(pc, draw);
    auto *ram = rt->memory.ram();
    for (uint32_t method : {0x129400u, 0x12a1a0u})
        for (uint32_t alignment = 0; alignment < 4; ++alignment)
            for (bool open : {false, true})
                for (uint32_t marker : {0u, 0xabcdef12u})
                {
                    ++cases;
                    word(ram, frame + 0x250, vtable); word(ram, vtable + 8, method);
                    word(ram, gp - 0x742c, packet);
                    word(ram, 0x1c75520, marker); word(ram, 0x402000, marker);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, output + alignment * 4); word(ram, packet + 4, output);
                    word(ram, packet + 8, open ? output - 16 : 0);
                    auto original = context(); packetCalls = drawCalls = 0;
                    FRAME_REFERENCE[0].function(ram, &original, rt.get());
                    check(original.pc == caller && !original.leaving, "original wrapper returns normally");
                    check(packetCalls == 2 && drawCalls == 1, "reference closes/reserves around one virtual draw");
                    const auto expectedArgs = drawArgs;
                    uint32_t expected; std::memcpy(&expected, ram + frame + 0x100, 4);
                    word(ram, frame + 0x100, 0);
                    word(ram, gp - 0x742c, 0x12000000);
                    std::memset(ram + packet, 0xa5, 0x20000);
                    std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                    word(expectedRam.data(), frame + 0x100, expected);
                    auto native = context(); native.r[5].uw[0] = 0x12000000;
                    const auto before = native; packetCalls = drawCalls = 0;
                    nativeDraw(ram, &native, rt.get());
                    check(native.pc == caller && !native.leaving, "native wrapper returns normally");
                    check(packetCalls == 0 && drawCalls == 1, "native dispatch needs no packet helpers");
                    check(drawArgs[0] == expectedArgs[0] && drawArgs[2] == expectedArgs[2] && drawArgs[1] == 0,
                          "same frame/RenderInfo dispatched with no console output");
                    if (wrapperPc == 0x12a110) check(native.r[2].uw[0] == original.r[2].uw[0], "member returns original zero word count");
                    for (int r = 16; r <= 31; ++r)
                        check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "callee-saved registers/stack/return intact");
                    check(std::memcmp(ram, expectedRam.data(), expectedRam.size()) == 0,
                          "logical side effect matches; packet and all other RAM untouched");
                }
    if (wrapperPc == 0x12ed80)
    {
        auto null = context(0); null.r[28].uw[0] = 0x12000000;
        const auto before = null; drawCalls = 0;
        nativeDraw(ram, &null, rt.get());
        check(null.pc == caller && !drawCalls && null.r[2].ud[0] == before.r[2].ud[0], "null MGDraw preserves void return and never reads packet/global state");
    }
    word(ram, vtable + 8, 0x123456);
    auto invalid = context(); drawCalls = 0;
    bool rejected = false;
    try { nativeDraw(ram, &invalid, rt.get()); } catch (const std::runtime_error &) { rejected = true; }
    check(rejected && !drawCalls, "unknown frame method rejected before dispatch");
    word(ram, vtable + 8, 0x129400); drawResult = 4;
    invalid = context(); rejected = false;
    try { nativeDraw(ram, &invalid, rt.get()); } catch (const std::runtime_error &) { rejected = true; }
    check(rejected, "unported console output is rejected explicitly");
    std::printf("DC frame dispatcher %x: %d original/native cases, %d failures\n", wrapperPc, cases, failures);
    return failures ? 1 : 0;
}
