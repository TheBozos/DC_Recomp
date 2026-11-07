// Whole loading callback: published native field query in both variants, exact
// production adaptations in the new one. Packet and SDK helpers are the real
// compiled/native implementations; sprite submission is a controlled boundary.
#include "dc_functions.h"
#include "loading_ref_functions.h"
#include "loading_native_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace dc { bool nativeInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }
namespace
{
    constexpr uint32_t packet = 0x1cfb370, output = 0x500000, gp = 0x300000, stack = 0x700000;
    int failures = 0, cases = 0;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n", cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    struct Event
    {
        uint32_t pc = 0;
        std::array<uint32_t, 10> data{};
        bool operator==(const Event &) const = default;
    };
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    struct Machine;
    struct NativePort : bozo::port::Host
    {
        bozo::libs::Libraries &libs;
        Machine *owner;
        NativePort(bozo::libs::Libraries &l, Machine *m) : libs(l), owner(m) {}
        bozo::libs::Libraries &libraries() override { return libs; }
        void frameEnd(Runtime *, R5900Context *) override { throw std::runtime_error("unexpected frame boundary"); }
        bool callLibrary(Runtime *, R5900Context *, uint32_t) override { return false; }
        bool nativeGraphics() const override { return true; }
        bool nativeInstruction(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc) override
        { return dc::nativeInstruction(ram, ctx, rt, pc); }
    };
    struct Observer : bozo::ee::KernelObserver
    {
        bozo::libs::Libraries &libs;
        explicit Observer(bozo::libs::Libraries &l) : libs(l) {}
        bool dmacStatusSet(uint32_t cause) override { return cause < 3 && libs.io.nativeCompleted[cause]; }
    };
    struct Profile { uint32_t title, stage; float colour, step; int32_t hold, countdown; bool ended; };
    struct Machine
    {
        Host host;
        bozo::ee::Dispatcher dispatcher{host};
        bozo::libs::Libraries libs;
        NativePort port{libs, this};
        Observer observer{libs};
        std::vector<BozoFunctionEntry> original;
        std::vector<Event> events;
        int packetCalls = 0, consoleCalls = 0, sendCalls = 0, syncCalls = 0;
        uint32_t sentSource = 0, sentChannel = 0;
        Machine()
        {
            auto &rt = dispatcher.runtime();
            rt.functions.setGenerated(dc_function_table, dc_function_count);
            for (uint32_t pc : {0x120c18u, 0x120de0u, 0x120c28u, 0x113e48u, 0x112ea0u, 0x1110b0u})
                original.push_back({pc, rt.functions.generated(pc)});
            for (uint32_t pc : {0x120c18u, 0x120de0u, 0x120c28u, 0x113e48u, 0x112ea0u, 0x1110b0u,
                  0x10cc58u, 0x15c310u, 0x1145d0u, 0x10c6f8u, 0x114c50u, 0x1126a8u, 0x112b20u})
                rt.functions.setOverride(pc, callback);
            libs.attach(dispatcher); rt.port = &port; dispatcher.kernel().setObserver(&observer);
            for (uint32_t ch : {1u, 2u})
            {
                auto &bank = libs.io.channels[ch];
                bank.chcr = bank.madr = bank.qwc = bank.tadr = 0xabcdef; bank.running = true;
            }
            libs.io.stat = 0x1234; libs.io.cpcondDone = 0x2345;
        }
        static void callback(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            auto &m = *static_cast<NativePort *>(rt->port)->owner;
            bozo::libs::Guest g(*rt, ctx);
            const uint32_t pc = ctx->pc;
            if (pc == 0x120c18 || pc == 0x120de0 || pc == 0x120c28 || pc == 0x113e48 || pc == 0x112ea0 || pc == 0x1110b0)
            {
                if (pc == 0x120c18 || pc == 0x120de0 || pc == 0x120c28) ++m.packetCalls;
                if (pc == 0x113e48 || pc == 0x112ea0) ++m.consoleCalls;
                for (const auto &f : m.original) if (f.address == pc) { f.function(ram, ctx, rt); return; }
                throw std::runtime_error("missing original loading helper");
            }
            if (pc == 0x114c50) { ++m.consoleCalls; g.ret(0); } // coherent native cache
            else if (pc == 0x1126a8) m.libs.find("sceGsPutDispEnv")->handler(m.libs, g);
            else if (pc == 0x112b20) m.libs.find("sceGsPutDrawEnv")->handler(m.libs, g);
            else
            {
                Event event{}; event.pc = pc;
                if (pc == 0x15c310)
                {
                    event.data[0] = g.arg(1); event.data[1] = g.arg(4);
                    std::memcpy(event.data.data() + 2, g.in(g.arg(2), 16), 16);
                    std::memcpy(event.data.data() + 6, g.in(g.arg(3), 16), 16);
                    g.write32(0x420000, event.data[0] ^ event.data[1]); g.ret(0);
                }
                else if (pc == 0x10cc58)
                {
                    ++m.syncCalls;
                    for (int i = 0; i < 3; ++i) event.data[i] = g.arg(i);
                    m.libs.find("sceDmaSync")->handler(m.libs, g);
                }
                else if (pc == 0x10c6f8)
                {
                    ++m.sendCalls; m.sentChannel = g.arg(0); m.sentSource = g.arg(1);
                    event.data[0] = g.arg(0); // packet/source is intentionally retired
                    m.libs.find("sceDmaSend")->handler(m.libs, g);
                }
                else if (pc == 0x1145d0) { event.data[0] = g.arg(0); g.ret(0); }
                else throw std::runtime_error("unexpected loading fixture call");
                m.events.push_back(event);
            }
            ctx->pc = ctx->r[31].uw[0];
        }
        void setup(const Profile &p, unsigned field, uint32_t frame, bool masked, bool adapted)
        {
            auto *ram = dispatcher.runtime().memory.ram();
            std::memset(ram + 0x1cfb140, 0xa5, 0x250);
            std::memset(ram + output, 0xa5, 0x10000);
            word(ram, packet, adapted ? 0x12000000 : output); word(ram, packet + 4, adapted ? 0x12000000 : output);
            word(ram, packet + 8, 0);
            word(ram, gp - 0x7920, p.ended); word(ram, gp - 0x7220, p.title); word(ram, gp - 0x7228, p.stage);
            word(ram, gp - 0x7230, std::bit_cast<uint32_t>(p.colour)); word(ram, gp - 0x722c, std::bit_cast<uint32_t>(p.step));
            word(ram, gp - 0x7224, static_cast<uint32_t>(p.hold)); word(ram, gp - 0x7234, static_cast<uint32_t>(p.countdown));
            word(ram, gp - 0x7440, frame); word(ram, gp - 0x7438, 0x10009000); word(ram, gp - 0x7434, 0x1000a000);
            word(ram, 0x420000, 0xa5a5a5a5);
            if (field) dispatcher.kernel().processEvents(dispatcher.kernel().nextVblankStart());
            dispatcher.kernel().addHandler(true, 1, 0x123400, -1, 0, 0, 0);
            dispatcher.kernel().causeEnable(true, 1, !masked);
        }
        R5900Context run(RecompiledFunction fn)
        {
            R5900Context ctx;
            for (int r = 16; r <= 23; ++r)
            {
                ctx.r[r].ud[0] = 0x1234567800000000ull + r;
                ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
            }
            ctx.r[28].uw[0] = gp; ctx.r[29].uw[0] = stack; ctx.r[31].uw[0] = 0x123450;
            const auto before = ctx;
            fn(dispatcher.runtime().memory.ram(), &ctx, &dispatcher.runtime());
            check(ctx.pc == 0x123450 && !ctx.leaving && ctx.r[2].ud[0] == 0, "loading callback returns zero normally");
            for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                check(std::memcmp(&ctx.r[r], &before.r[r], sizeof(ctx.r[r])) == 0, "full saved registers/stack/return preserved");
            return ctx;
        }
        void checkBank()
        {
            for (uint32_t ch : {1u, 2u})
            {
                const auto &b = libs.io.channels[ch];
                check(b.chcr == 0xabcdef && b.madr == 0xabcdef && b.qwc == 0xabcdef && b.tadr == 0xabcdef && b.running,
                      "retired channel bank untouched");
            }
            check(libs.io.stat == 0x1234 && libs.io.cpcondDone == 0x2345, "retired status/condition bank untouched");
        }
    };
}
int main()
{
    const Profile profiles[] = {
        {0,0,64,1,0,0,true}, {0,0,64,1,0,1,false}, {0,0,64,1,0,2,false},
        {0,0,-1,-1,0,0,false}, {0,0,129,1,0,0,false}, {0,0,127,1,0,0,false},
        {0x321,0,-1,-1,0,0,false}, {0x321,1,-1,-1,0,0,false}, {0x321,2,-1,-1,0,0,false},
        {0x321,0,129,1,0,0,false}, {0x321,0,64,1,1,0,false}, {0,0,64,1,0,-1,false}
    };
    for (const auto &p : profiles)
        for (unsigned field : {0u, 1u})
            for (uint32_t frame : {0u, 1u})
                for (bool masked : {false, true})
                {
                    ++cases;
                    auto ref = std::make_unique<Machine>(), native = std::make_unique<Machine>();
                    ref->setup(p, field, frame, masked, false); native->setup(p, field, frame, masked, true);
                    const auto original = ref->run(loading_ref_function_table[0].function);
                    auto *ram = native->dispatcher.runtime().memory.ram();
                    const auto *expected = ref->dispatcher.runtime().memory.ram();
                    std::vector<uint8_t> expectedRam(ram, ram + bozo::kFlatScratchpadOffset);
                    std::memcpy(expectedRam.data() + gp - 0x8000, expected + gp - 0x8000, 0x8100);
                    std::memcpy(expectedRam.data() + 0x420000, expected + 0x420000, 4);
                    const auto adapted = native->run(loading_native_function_table[0].function);
                    const bool submitted = !p.ended && p.countdown == 0;
                    check(ref->packetCalls == (submitted ? 4 : 0) && native->packetCalls == 0,
                          "only reference resets/ends/terminates, including End's now-visible nested terminate");
                    check(ref->consoleCalls == (p.ended ? 0 : 3) && native->consoleCalls == 0, "adapted callback skips console half-offset/cache/swap setup");
                    const int expectedSyncs = p.ended ? 0 : p.countdown == 0 ? 2 : 1;
                    check(ref->events == native->events && ref->syncCalls == expectedSyncs && ref->syncCalls == native->syncCalls,
                          "fade/sprite inputs, completion/cache/sync arguments and ordering match");
                    check(ref->sendCalls == submitted && native->sendCalls == submitted &&
                          (!submitted || (ref->sentChannel == native->sentChannel && ref->sentSource == output && native->sentSource == 0)),
                          "same native completion count/channel without a packet source");
                    check(ref->libs.io.nativeSubmissions[1] == submitted && native->libs.io.nativeSubmissions[1] == submitted &&
                          ref->libs.io.nativeCompleted[1] == submitted && native->libs.io.nativeCompleted[1] == submitted,
                          "native completion ownership matches");
                    const bool queued = submitted && !masked;
                    check(ref->dispatcher.kernel().handlerQueued() == queued && native->dispatcher.kernel().handlerQueued() == queued,
                          "completion IRQ respects callback mask");
                    if (submitted && masked)
                    {
                        ref->dispatcher.kernel().causeEnable(true, 1, true); native->dispatcher.kernel().causeEnable(true, 1, true);
                        check(ref->dispatcher.kernel().handlerQueued() && native->dispatcher.kernel().handlerQueued(), "masked completion survives until enabled");
                    }
                    check(adapted.fcr31 == original.fcr31, "fade float status matches published reference");
                    check(std::memcmp(ram, expectedRam.data(), stack - 0x1000) == 0 &&
                          std::memcmp(ram + stack, expectedRam.data() + stack, expectedRam.size() - stack) == 0,
                          "logical state/draw effects match; private packet, display records and all other non-stack RAM untouched");
                    ref->checkBank(); native->checkBank();
                }
    std::printf("DC loading callback: %d published/adapted cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
