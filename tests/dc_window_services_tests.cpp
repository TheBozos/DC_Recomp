// Compare logical window metadata with compiled originals; no game data embedded.
#include "dc_functions.h"
#include "bozo/port.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    int failures = 0, cases = 0;
    constexpr uint32_t rectangle = 0x01c75510, input = 0x400000;
    constexpr uint32_t packet = 0x500000, buffer = 0x510000, gp = 0x300000, stack = 0x700000;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); }
    R5900Context context(uint32_t pc, uint32_t source)
    {
        R5900Context ctx;
        ctx.pc = pc;
        ctx.r[4].uw[0] = source;
        ctx.r[28].uw[0] = gp;
        ctx.r[29].uw[0] = stack;
        ctx.r[31].uw[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    auto rt = std::make_unique<Runtime>();
    rt->functions.setGenerated(dc_function_table, dc_function_count);
    bozo::port::Registry registry;
    dc::native::install(*rt, registry);
    auto *ram = rt->memory.ram();
    // Signed/full-width coordinates must survive independently of the discarded
    // scissor packing. Include source == destination to check value semantics.
    for (uint32_t pc : {0x12d9a0u, 0x12dae0u})
        for (uint32_t source : {input, rectangle})
            for (uint32_t x : {0u, 1u, 0xffffffffu, 0x80000000u, 0x7fffffffu})
                for (uint32_t width : {0u, 640u, 0xffffffffu, 0x80000000u})
                {
                    ++cases;
                    const std::array<uint32_t, 4> value{x, x ^ 0xa5010000u, width, width ^ 0x5a00ffffu};
                    std::memset(ram + rectangle, 0x6b, 16);
                    std::memcpy(ram + source, value.data(), 16);
                    std::memset(ram + packet, 0, 0x20000);
                    word(ram, packet, buffer); word(ram, packet + 4, buffer);
                    word(ram, gp - 0x742c, packet);
                    auto original = context(pc, source);
                    rt->functions.generated(pc)(ram, &original, rt.get());
                    check(original.pc == 0x123450, "original setter returns");
                    std::array<uint32_t, 4> expected;
                    std::memcpy(expected.data(), ram + rectangle, 16);

                    std::memset(ram + rectangle, 0x6b, 16);
                    std::memcpy(ram + source, value.data(), 16);
                    std::memset(ram + packet, 0xa5, 0x20000);
                    std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                    std::memcpy(expectedRam.data() + rectangle, expected.data(), 16);
                    auto native = context(pc, pc == 0x12d9a0 ? 0x12000000 : source);
                    native.r[28].uw[0] = 0x12000000; // no packet/global-pointer dependency
                    registry.find(pc)(ram, &native, rt.get());
                    check(std::memcmp(ram, expectedRam.data(), expectedRam.size()) == 0,
                          "only the original logical rectangle changes; packet output remains untouched");
                    check(native.pc == 0x123450 && native.r[29].uw[0] == stack,
                          "native setter preserves return and stack");
                }
    std::printf("DC window services: %d original/native cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
