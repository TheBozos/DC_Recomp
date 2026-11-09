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
        // MGInit's material defaults, once read from the draw record sceGsSetDefDBuff made (no longer made): draw 0's
        // TEST (Z test on, GEQUAL; MGInit then sets the alpha test itself) and ZBUF (Z written). setGsReg3Hook reads only
        // their alpha-test and Z-mask fields.
        if (pc == 0x0012C510u) // ld a2,0(a2)
        {
            ctx->r[6].ud[0] = 0x50000u;
            return true;
        }
        if (pc == 0x0012C57Cu) // ld a0,0(a0)
        {
            ctx->r[4].ud[0] = 0x100008Cu;
            return true;
        }
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
    // FlushCache, iFlushCache and iSyncDCache: the game writes the PS2's data cache back before its transfers and in
    // its vblank callbacks. The PC has no such cache to keep coherent with any device: nothing, 0, without a kernel
    // call (each caller still makes its call, with its checkpoint).
    void noCache(uint8_t *, R5900Context *ctx, Runtime *)
    {
        ctx->r[2].ud[0] = 0;
        ctx->pc = ctx->r[31].uw[0];
    }
    // sceGsSyncV(mode): the game waits for the next vblank (boot, loading screens, mode changes), and its frame ends
    // there. As the engine's frameBoundary stand-in did: the parity of the frames ended by then, this one included,
    // then the frame boundary.
    void syncV(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        const uint64_t parity = (rt->port->libraries().framesEnded + 1u) & 1u;
        ctx->r[2].ud[0] = parity;
        bozo::port::frameEnd(rt, ctx);
        ctx->pc = ctx->r[31].uw[0];
    }
    // sceGsResetGraph(mode, interlace, video mode, field mode): mode 0 sets the display mode, whose only effect left is
    // the field clock the game runs on (NTSC 59.94 Hz; PAL 50 Hz for video mode 3). No GS, no display registers; the
    // parameter words the GS stand-ins read are no longer read by anything.
    void resetGraph(uint8_t *, R5900Context *ctx, Runtime *rt)
    {
        if (static_cast<int16_t>(ctx->r[4].uw[0]) == 0)
        {
            auto &kernel = bozo::port::dispatcher(rt).kernel();
            if ((ctx->r[6].uw[0] & 0xffu) == 3u)
                kernel.fieldRate(50, 1);
            else
                kernel.fieldRate(60000, 1001);
        }
        ctx->r[2].ud[0] = 0;
        ctx->pc = ctx->r[31].uw[0];
    }
    void install(Runtime &rt, bozo::port::Registry &r)
    {
        r.registerFunction(0x112f00, syncV);      // sceGsSyncV
        r.registerFunction(0x1122b0, resetGraph); // sceGsResetGraph
        dc::native::install(rt, r);
        r.registerFunction(0x1145a0, noCache); // FlushCache
        r.registerFunction(0x1145d0, noCache); // iFlushCache
        r.registerFunction(0x114c50, noCache); // iSyncDCache
        r.registerFunction(0x12d070, frameEnd);
        r.nativeGraphics(&nativeInstruction);
    }
} // namespace dc
