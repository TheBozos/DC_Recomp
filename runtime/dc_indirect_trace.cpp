#include "dc_indirect_trace.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <tuple>
#ifdef DC_TRACE_GAME
#include "bozo/runtime.h"
#include "title_functions.h"
#include "dun_functions.h"
#endif
namespace
{
    using Key = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
    struct Entry { uint64_t count = 0; std::array<uint32_t, 4> first{}, last{}; };
    std::map<Key, Entry> entries;
    std::mutex mutex;
    const char *path() { static const char *value = std::getenv("DC_INDIRECT_TRACE"); return value; }
    uint32_t targetImage(Runtime *rt, uint32_t target)
    {
        if (target < 0x1dabd00 || target >= 0x1f00000) return 0;
#ifdef DC_TRACE_GAME
        // Compare the selected compiled resume entry, never guess which image
        // owns a reused address from the caller's module or stale cached state.
        if (rt)
            if (const auto *selected = rt->functions.resumeEntry(target))
            {
                for (size_t i = 0; i < title_resume_count; ++i)
                    if (selected == &title_resume_table[i]) return 1;
                for (size_t i = 0; i < dun_resume_count; ++i)
                    if (selected == &dun_resume_table[i]) return 2;
            }
#else
        (void)rt;
#endif
        return 3; // no selected image: preserve the uncertainty in the report
    }
}
uint32_t dcTraceIndirect(Runtime *rt, const R5900Context *ctx, uint32_t image, uint32_t pc, uint32_t target)
{
    if (path() && *path())
    {
        std::lock_guard lock(mutex);
        auto &entry = entries[{image, pc, targetImage(rt, target), target}];
        std::array<uint32_t, 4> args{};
        for (int i = 0; i < 4; ++i) args[i] = ctx->r[4 + i].uw[0];
        if (!entry.count++) entry.first = args;
        entry.last = args;
    }
    return target;
}
bool dcFinishIndirectTrace(bool complete)
{
    if (!path() || !*path()) return true;
    std::lock_guard lock(mutex);
    std::ofstream out(path());
    out << "{\"schema\":1,\"complete\":" << (complete ? "true" : "false") << ",\"edges\":[";
    bool comma = false;
    for (const auto &[key, entry] : entries)
    {
        if (comma) out << ','; comma = true;
        const auto &[image, pc, targetImage, target] = key;
        out << "{\"image\":" << image << ",\"pc\":" << pc << ",\"target_image\":" << targetImage << ",\"target\":" << target << ",\"count\":" << entry.count;
        for (const auto &[name, args] : {std::pair{"first_args", entry.first}, std::pair{"last_args", entry.last}})
        {
            out << ",\"" << name << "\":[";
            for (int i = 0; i < 4; ++i) { if (i) out << ','; out << args[i]; }
            out << ']';
        }
        out << '}';
    }
    out << "]}\n"; out.close();
    if (!out) std::fprintf(stderr, "[dc] failed to write indirect audit trace\n");
    return bool(out);
}
