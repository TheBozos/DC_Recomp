#include "dc_indirect_trace.h"
#include <cstring>
#ifdef DC_TRACE_GAME
#include "bozo/runtime.h"
#include "title_functions.h"
#include "dun_functions.h"
#include <memory>
#endif
int main()
{
    R5900Context ctx;
    ctx.pc = 0x111111; ctx.r[31].uw[0] = 0x222222; ctx.r[4].uw[0] = 7;
    auto before = ctx;
    if (dcTraceIndirect(nullptr, &ctx, 0, 0x1000, 0x1234) != 0x1234 || std::memcmp(&ctx, &before, sizeof(ctx))) return 1;
    ctx.r[4].uw[0] = 9; before = ctx;
    if (dcTraceIndirect(nullptr, &ctx, 0, 0x1000, 0x1234) != 0x1234 || std::memcmp(&ctx, &before, sizeof(ctx))) return 1;
    if (dcTraceIndirect(nullptr, &ctx, 1, 0x1000, 0x1234) != 0x1234 || std::memcmp(&ctx, &before, sizeof(ctx))) return 1;
#ifdef DC_TRACE_GAME
    auto rt = std::make_unique<Runtime>();
    auto *identity = rt->memory.ram() + 0x1dabd00;
    rt->functions.addModule(0x1dabd00,0x1f00000,identity,{0x55},title_function_table,title_function_count,title_resume_table,title_resume_count);
    rt->functions.addModule(0x1dabd00,0x1f00000,identity,{0xaa},dun_function_table,dun_function_count,dun_resume_table,dun_resume_count);
    *identity = 0x55;
    if (dcTraceIndirect(rt.get(), &ctx, 0, 0x2000, 0x1dabd80) != 0x1dabd80) return 1;
    *identity = 0xaa;
    if (dcTraceIndirect(rt.get(), &ctx, 0, 0x2000, 0x1dabd80) != 0x1dabd80) return 1;
    if (std::memcmp(&ctx, &before, sizeof(ctx))) return 1;
#endif
    return dcFinishIndirectTrace(true) ? 0 : 1;
}
