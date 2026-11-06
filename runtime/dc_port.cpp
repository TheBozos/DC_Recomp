// Dark Cloud's native port boundary. No hardware execution or interpreter fallback.
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include <cstring>
namespace dc::native
{
    void install(Runtime &, bozo::port::Registry &);
    void publish();
} // namespace dc::native
namespace dc
{
    void frameEnd(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        auto &d = bozo::port::dispatcher(rt);
        const auto count = static_cast<uint32_t>(d.kernel().vblankCount());
        std::memcpy(ram + 0x2a2400, &count, 4);
        dc::native::publish();
        bozo::port::frameEnd(rt, ctx);
        ctx->pc = ctx->r[31].uw[0];
    }
    void install(Runtime &rt, bozo::port::Registry &r)
    {
        dc::native::install(rt, r);
        r.registerFunction(0x12d070, frameEnd);
    }
} // namespace dc
