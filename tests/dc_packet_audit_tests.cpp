// Export actual installed overrides, and test fail-closed packet/readback
// boundaries with inaccessible pointers before any guest state can be touched.
#include "bozo/port.h"
#ifdef DC_PACKET_AUDIT_GAME
#include "dc_functions.h"
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
int main(int argc, char **argv)
{
    // Audit the ordinary install, independently of the caller's diagnostic env.
    for (const char *name : {"DC_SKIP_FN", "DC_NO_DOF", "DC_PROBE_TEX", "DC_PROBE_FEEDERS", "DC_PROBE_VU1"})
    {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }
    auto rt = std::make_unique<Runtime>();
#ifdef DC_PACKET_AUDIT_GAME
    rt->functions.setGenerated(dc_function_table, dc_function_count);
#endif
    bozo::port::Registry registry; dc::native::install(*rt, registry);
    for (auto entry : registry.functions()) std::printf("override 0x%x\n", entry.address);
    // The feeder probe calls some generated originals. It must never resurrect
    // a retired builder/readback owner over the production rejection boundary.
#ifdef _WIN32
    _putenv_s("DC_PROBE_FEEDERS", "1");
#else
    setenv("DC_PROBE_FEEDERS", "1", 1);
#endif
    bozo::port::Registry diagnostic; dc::native::install(*rt, diagnostic);
    auto *ram = rt->memory.ram();
    std::memset(ram, 0xa5, bozo::kFlatScratchpadOffset);
    const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
    for (int i = 1; i < argc; ++i)
    {
        char *end = nullptr; const uint32_t pc = std::strtoul(argv[i], &end, 0);
        if (!end || *end || !registry.find(pc) || registry.find(pc) != diagnostic.find(pc)) return 1;
        R5900Context ctx; ctx.pc = pc;
        for (auto &r : ctx.r) { r.ud[0] = 0x12000000; r.ud[1] = 0x12345678abcdef01ull; }
        const auto before = ctx; bool rejected = false;
        try { registry.find(pc)(ram, &ctx, rt.get()); }
        catch (const std::runtime_error &e)
        {
            const std::string message = e.what();
            rejected = message.starts_with("native graphics: retired packet builder reached") ||
                message == "native graphics: console StoreImage requires a native screenshot owner" ||
                message == "native graphics: legacy balloon helper requires the native MakeFukidashi owner";
        }
        if (!rejected || std::memcmp(ram, initial.data(), initial.size()) || std::memcmp(&ctx, &before, sizeof(ctx)))
            return 1;
        std::printf("reject 0x%x\n", pc);
    }
    return 0;
}
