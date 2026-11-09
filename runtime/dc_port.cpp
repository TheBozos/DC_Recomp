// Dark Cloud's native port boundary. No hardware execution or interpreter fallback.
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "dc_sound.h"
#include "dc_visibility.h"
#include <cstring>
namespace dc::native
{
    void install(Runtime &, bozo::port::Registry &);
    void publish();
    void fishLineVertex(uint8_t *, R5900Context *, Runtime *);
    void debugFontBoundary(uint8_t *, R5900Context *, Runtime *, uint32_t);
} // namespace dc::native
namespace dc
{
    bool nativeInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc)
    {
        if (dc::sound::nativeSite(ram, ctx, rt, pc))
            return true;
        if (pc == 0x0013DFA8u || pc == 0x0013DFF0u || pc == 0x0013E314u || pc == 0x0013E420u)
        {
            dc::native::debugFontBoundary(ram, ctx, rt, pc);
            return true;
        }
        if (pc == 0x001AB290u)
        {
            dc::native::fishLineVertex(ram, ctx, rt);
            return true;
        }
        if (pc == 0x0012D268u)
        {
            dc::native::queryVisibility(*rt, *ctx);
            return true;
        }
        if (pc != 0x0012C19Cu && pc != 0x0015434Cu)
            return false;
        // The two vblank callbacks need logical field parity, not a GS CSR read.
        // The guarded build-time adaptations remove their obsolete >>13 extraction.
        ctx->r[2].ud[0] = bozo::port::vblankCount(rt) & 1u;
        return true;
    }
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
        r.nativeGraphics(&nativeInstruction);
    }
} // namespace dc
