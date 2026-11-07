// Game-free regression: native draw/material calls must not construct console packets.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/ee/dispatcher.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace dc { bool nativeInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }
namespace
{
    int failures = 0;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &what) override { throw std::runtime_error(what); }
    };
}
int main()
{
    auto runtime = std::make_unique<Runtime>();
    // Rendering and asset bookkeeping install without any original packet producer.
    bozo::port::Registry registry;
    dc::native::install(*runtime, registry);
    const auto material = registry.find(0x129360);
    const auto draw = registry.find(0x135000);
    check(material != nullptr && draw != nullptr, "native hooks install without packet producer functions");
    if (!material || !draw) return 1;

    auto *ram = runtime->memory.ram();
    constexpr auto ramBytes = bozo::kFlatScratchpadOffset;
    std::memset(ram + 0x100000, 0xa5, 4096);
    const uint64_t test = 1u | (6u << 1) | (32u << 4), zbuf = 1ull << 32, alpha = 0x44;
    std::memcpy(ram + 0x102000, &test, 8);
    std::memcpy(ram + 0x102008, &zbuf, 8);
    std::memcpy(ram + 0x102010, &alpha, 8);
    const std::vector<uint8_t> beforeMaterial(ram, ram + ramBytes);
    const auto packetGetter = registry.find(0x12e280);
    check(packetGetter != nullptr, "null compatibility getter is installed");
    if (packetGetter)
    {
        R5900Context get;
        get.pc = 0x12e280; get.r[28].uw[0] = 0x12000000;
        get.r[29].uw[0] = 0x12000000; get.r[31].uw[0] = 0x123400;
        packetGetter(ram, &get, runtime.get());
        check(get.pc == 0x123400 && get.r[2].ud[0] == 0 && get.r[28].uw[0] == 0x12000000,
              "getter returns null without a readable GP/stack/packet object");
        check(std::memcmp(ram, beforeMaterial.data(), ramBytes) == 0, "getter leaves all guest RAM untouched");
    }
    R5900Context ctx;
    ctx.r[4].uw[0] = 0x100000;
    ctx.r[5].uw[0] = 0x102000;
    ctx.r[6].uw[0] = 0x102008;
    ctx.r[7].uw[0] = 0x102010;
    ctx.r[31].uw[0] = 0x123400;
    material(ram, &ctx, runtime.get());
    check(ctx.pc == 0x123400 && ctx.r[2].sw[0] == 0, "native material returns without reserving packet words");
    check(std::memcmp(ram, beforeMaterial.data(), ramBytes) == 0, "material call leaves guest RAM unchanged");

    // A visual with data enters the model path; an unknown MDT is handled without touching
    // its packet buffer. All inputs live away from the guard region.
    const uint32_t data = 0x105000, qwords = 1;
    std::memcpy(ram + 0x103018, &data, 4);
    std::memcpy(ram + 0x10301c, &qwords, 4);
    const std::vector<uint8_t> beforeDraw(ram, ram + ramBytes);
    ctx.r[4].uw[0] = 0x103000;
    ctx.r[5].uw[0] = 0x100000;
    ctx.r[6].uw[0] = 0x104000;
    ctx.r[7].uw[0] = 0x106000;
    ctx.r[8].uw[0] = 0;
    ctx.r[11].uw[0] = 0;
    for (uint32_t pc : {0x135000u, 0x134bc0u})
    {
        const auto hook = registry.find(pc);
        check(hook != nullptr, "both native model overloads install without original packet wrappers");
        if (!hook) continue;
        ctx.pc = pc;
        ctx.r[5].uw[0] = 0x12000000; // neither raw output nor packet overload reads this address
        hook(ram, &ctx, runtime.get());
        check(ctx.pc == 0x123400 && ctx.r[2].sw[0] == 0, "native model draw returns without packet words");
        check(std::memcmp(ram, beforeDraw.data(), ramBytes) == 0, "model draw leaves guest RAM and packet guard unchanged");
    }

    ctx.r[28].uw[0] = 0x110000;
    std::memcpy(ram + 0x110000 - 0x7408, &alpha, 8);
    const std::vector<uint8_t> beforeEnvironment(ram, ram + ramBytes);
    for (uint32_t address : {0x140da0u, 0x15bc20u, 0x12ef90u, 0x12edf0u, 0x12eec0u,
                             0x12f060u, 0x13dea0u, 0x13df00u})
    {
        const auto hook = registry.find(address);
        check(hook != nullptr, "native environment hooks install without packet producer functions");
        if (!hook) continue;
        ctx.pc = address;
        const bool packetOnly = address == 0x12edf0 || address == 0x12eec0 || address == 0x12f060 ||
                                address == 0x13dea0 || address == 0x13df00;
        ctx.r[4].uw[0] = packetOnly ? 0x12000000 : address == 0x12ef90 ? 0x102010 : 0x100000;
        ctx.r[5].uw[0] = 0x102010;
        hook(ram, &ctx, runtime.get());
        check(ctx.pc == 0x123400, "native environment returns to its caller");
        check(std::memcmp(ram, beforeEnvironment.data(), ramBytes) == 0,
              "native environment/alpha setter leaves packet buffers and guest RAM unchanged");
    }
    Host host;
    bozo::ee::Dispatcher dispatcher(host);
    for (unsigned field = 0; field < 6; ++field)
    {
        if (field) dispatcher.kernel().processEvents(dispatcher.kernel().nextVblankStart());
        for (uint32_t pc : {0x12c19cu, 0x15434cu})
        {
            ctx.pc = 0x123450;
            ctx.r[31].ud[0] = 0x987650;
            check(dc::nativeInstruction(dispatcher.runtime().memory.ram(), &ctx, &dispatcher.runtime(), pc),
                  "both callbacks use the native vblank field service");
            check(ctx.r[2].ud[0] == (dispatcher.kernel().vblankCount() & 1u), "native field matches logical vblank parity");
            check(ctx.pc == 0x123450 && ctx.r[31].ud[0] == 0x987650, "inline field query preserves control flow");
        }
    }
    check(!dc::nativeInstruction(nullptr, &ctx, &dispatcher.runtime(), 0), "unregistered native site is declined");
    std::printf("DC native packet boundary: %d failures\n", failures);
    return failures ? 1 : 0;
}
