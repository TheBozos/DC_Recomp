// Mid-function reference/adapted spans, using real native SDK completion services.
#include "dc_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t packet = 0x500000, output = 0x510000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0;
    void check(bool ok, const char *message)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, message); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t value) { std::memcpy(ram + at, &value, 4); }
    uint32_t word(const uint8_t *ram, uint32_t at) { uint32_t value; std::memcpy(&value, ram + at, 4); return value; }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected SDK call"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0 call"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    struct Machine;
    struct NativePort : bozo::port::Host
    {
        bozo::libs::Libraries &l;
        Machine *owner;
        NativePort(bozo::libs::Libraries &libs, Machine *machine) : l(libs), owner(machine) {}
        bozo::libs::Libraries &libraries() override { return l; }
        void frameEnd(Runtime *, R5900Context *) override { throw std::runtime_error("unexpected frame boundary"); }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { return false; }
        bool nativeGraphics() const override { return true; }
    };
    struct Observer : bozo::ee::KernelObserver
    {
        bozo::libs::Libraries &l;
        explicit Observer(bozo::libs::Libraries &libs) : l(libs) {}
        bool dmacStatusSet(uint32_t cause) override { return cause < 3 && l.io.nativeCompleted[cause]; }
    };
    struct Machine
    {
        Host host;
        bozo::ee::Dispatcher dispatcher{host};
        bozo::libs::Libraries libs;
        NativePort port{libs, this};
        Observer observer{libs};
        RecompiledFunction end = nullptr, terminate = nullptr, channel = nullptr;
        int packetCalls = 0, syncCalls = 0, flushCalls = 0, sendCalls = 0, dmaSyncCalls = 0, gsCalls = 0, channelCalls = 0;
        uint32_t sentChannel = 0, sentSource = 0;
        Machine()
        {
            auto &rt = dispatcher.runtime();
            rt.functions.setGenerated(dc_function_table, dc_function_count);
            end = rt.functions.generated(0x120de0); terminate = rt.functions.generated(0x120c28);
            channel = rt.functions.generated(0x10c3a8);
            if (!end || !terminate || !channel) throw std::runtime_error("missing original SDK helper");
            for (uint32_t pc : {0x120de0u,0x120c28u,0x112f98u,0x104698u,0x113f40u,0x10c3a8u,0x1145a0u,0x10c6f8u,0x10cc58u,0x113e48u,0x112ea0u})
                rt.functions.setOverride(pc, sdk);
            libs.attach(dispatcher); rt.port = &port; dispatcher.kernel().setObserver(&observer);
            auto &old = libs.io.channels[1];
            old.chcr = old.madr = old.qwc = old.tadr = 0xabcdef; old.running = true;
            libs.io.stat = 0x1234; libs.io.cpcondDone = 0x2345;
            word(rt.memory.ram(), 0x2502d4, 0x10009000); // original SDK's opaque channel-1 handle table
        }
        static void sdk(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            auto &m = *static_cast<NativePort *>(rt->port)->owner;
            bozo::libs::Guest guest(*rt, ctx);
            const uint32_t pc = ctx->pc;
            if (pc == 0x120de0 || pc == 0x120c28)
            {
                ++m.packetCalls; (pc == 0x120de0 ? m.end : m.terminate)(ram, ctx, rt);
            }
            else if (pc == 0x112f98)
            {
                ++m.syncCalls;
                m.libs.find("sceGsSyncPath")->handler(m.libs, guest);
            }
            else if (pc == 0x10c3a8) { ++m.channelCalls; m.channel(ram, ctx, rt); }
            else if (pc == 0x113e48 || pc == 0x112ea0) ++m.gsCalls; // sceGsSetHalfOffset / sceGsSwapDBuff
            else if (pc == 0x10cc58)
            {
                ++m.dmaSyncCalls;
                m.libs.find("sceDmaSync")->handler(m.libs, guest);
            }
            else if (pc == 0x1145a0) { ++m.flushCalls; guest.ret(0); } // coherent native RAM, no cache device
            else if (pc == 0x10c6f8)
            {
                ++m.sendCalls; m.sentChannel = guest.arg(0); m.sentSource = guest.arg(1);
                m.libs.find("sceDmaSend")->handler(m.libs, guest);
            }
            else throw std::runtime_error("native graphics path unexpectedly entered its obsolete SDK error branch");
            ctx->pc = ctx->r[31].uw[0];
        }
        void setup(uint32_t cursor, bool open, uint32_t count, uint32_t frame, bool inaccessible)
        {
            auto *ram = dispatcher.runtime().memory.ram();
            std::memset(ram + packet, 0xa5, 0x20000);
            word(ram, packet, output + 32 + cursor); word(ram, packet + 4, output);
            word(ram, packet + 8, open ? output : 0);
            word(ram, gp - 0x742c, inaccessible ? 0x12000000 : packet);
            word(ram, gp - 0x73ac, count); word(ram, gp - 0x7440, frame);
        }
        R5900Context run(RecompiledFunction fn)
        {
            R5900Context ctx;
            ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
            fn(dispatcher.runtime().memory.ram(), &ctx, &dispatcher.runtime());
            check(ctx.pc == 0x123450 && !ctx.leaving && ctx.r[29].uw[0] == stack,
                  "isolated span returns normally with its stack intact");
            return ctx;
        }
        void checkRetiredBank()
        {
            const auto &old = libs.io.channels[1];
            check(old.chcr == 0xabcdef && old.madr == 0xabcdef && old.qwc == 0xabcdef && old.tadr == 0xabcdef && old.running &&
                  libs.io.stat == 0x1234 && libs.io.cpcondDone == 0x2345, "native SDK work leaves the retired device bank unchanged");
        }
    };
}
int main()
{
#ifdef DC_FRAME_CLOSE
    for (uint32_t cursor : {0u,4u,8u,12u})
        for (bool open : {false,true})
        {
            ++cases; Machine reference, native;
            reference.setup(cursor, open, 0, 0, false); native.setup(cursor, open, 0, 0, true);
            reference.run(DC_FRAME_REFERENCE[0].function);
            // SDK calls are now externals too: End's nested Terminate is visible.
            check(reference.packetCalls == 3 && reference.syncCalls == 1,
                  "published reference closes a packet and calls the always-idle native sync service");
            auto *ram = native.dispatcher.runtime().memory.ram();
            const std::vector<uint8_t> before(ram, ram + bozo::kFlatScratchpadOffset);
            native.run(DC_FRAME_NATIVE[0].function);
            check(native.packetCalls == 0 && native.syncCalls == 0 && native.sendCalls == 0,
                  "adapted close span needs no packet, poll or fabricated completion");
            check(std::memcmp(ram, before.data(), before.size()) == 0, "adapted close span leaves ALL guest RAM unchanged");
            reference.checkRetiredBank(); native.checkRetiredBank();
        }
    std::printf("DC frame close: %d reference/adapted cases, %d failures\n", cases, failures);
#else
    // The per-frame transfer tail: the published reference waits on the GIF channel, flushes and sends an empty VIF1
    // transfer; the adapted tail does none of it. The game installs no DMA-completion handler, so no completion is
    // owed. Frame counter, its first-frame flag, channel lookup and buffer toggle must match.
    for (uint32_t count : {0u,1u,0x7fffffffu,0xffffffffu})
        for (uint32_t frame : {0u,1u,0xffffffffu})
            for (bool first : {false,true})
            {
                ++cases; Machine reference, native;
                reference.setup(0, false, count, frame, false); native.setup(0, false, count, frame, true);
                for (Machine *m : {&reference, &native})
                {
                    auto *r = m->dispatcher.runtime().memory.ram();
                    r[gp - 0x73a8] = first ? 0 : 1;
                    word(r, gp - 0x7434, 0x1000a000); // the GIF and VIF1 channel handles MGInit stored
                    word(r, gp - 0x7438, 0x10009000);
                }
                reference.run(DC_FRAME_REFERENCE[0].function);
                auto *ram = native.dispatcher.runtime().memory.ram();
                const auto *ref = reference.dispatcher.runtime().memory.ram();
                std::vector<uint8_t> expected(ram, ram + bozo::kFlatScratchpadOffset);
                for (uint32_t address : {gp - 0x73ac, gp - 0x7438, gp - 0x7440}) word(expected.data(), address, word(ref, address));
                expected[gp - 0x73a8] = ref[gp - 0x73a8];
                native.run(DC_FRAME_NATIVE[0].function);
                check(reference.gsCalls == 2 && reference.dmaSyncCalls == 1 && reference.flushCalls == 2 &&
                      reference.channelCalls == 1 && reference.sendCalls == 1,
                      "published reference sets the half offset, swaps, waits, flushes twice, looks the channel up and sends");
                check(word(ref, gp - 0x7438) == 0x10009000, "the reference's lookup returns the handle MGInit stored");
                check(native.gsCalls == 0 && native.channelCalls == 0 && native.dmaSyncCalls == 0 && native.flushCalls == 0 && native.sendCalls == 0 &&
                      native.libs.io.nativeSubmissions[1] == 0 && native.libs.io.nativeSynchronizations == 0,
                      "adapted tail makes no transfer, flush or wait");
                check(reference.sentChannel == 0x10009000 && reference.sentSource == output,
                      "the reference sent the old packet on the VIF1 channel");
                check(!native.dispatcher.kernel().handlerQueued(), "no completion callback is queued");
                check(std::memcmp(ram, expected.data(), expected.size()) == 0,
                      "frame counter, its flag, channel handle and buffer toggle match; ALL other guest RAM stays unchanged");
                reference.checkRetiredBank(); native.checkRetiredBank();
            }
    std::printf("DC frame submit: %d reference/adapted cases, %d failures\n", cases, failures);
#endif
    return failures ? 1 : 0;
}
