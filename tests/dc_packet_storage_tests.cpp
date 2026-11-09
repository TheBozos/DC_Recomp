// Actual arena allocator and whole retail buffer owners, before/after exact
// production patches. Only diagnostics/SDK channel lookup are controlled.
#include "dc_functions.h"
#if defined(DC_PACKET_RESERVE)
#include "packet_reserve_ref_functions.h"
#include "packet_reserve_native_functions.h"
#define REFERENCE packet_reserve_ref_function_table
#define ADAPTED packet_reserve_native_function_table
constexpr int kind = 0;
#elif defined(DC_BUFFER_CLEAR)
#include "buffer_clear_ref_functions.h"
#include "buffer_clear_native_functions.h"
#define REFERENCE buffer_clear_ref_function_table
#define ADAPTED buffer_clear_native_function_table
constexpr int kind = 1;
#elif defined(DC_PACKET_HANDLE)
#include "packet_handle_ref_functions.h"
#include "packet_handle_native_functions.h"
#define REFERENCE packet_handle_ref_function_table
#define ADAPTED packet_handle_native_function_table
constexpr int kind = 2;
#else
#include "packet_get_ref_functions.h"
#include "packet_get_native_functions.h"
#define REFERENCE packet_get_ref_function_table
#define ADAPTED packet_get_native_function_table
constexpr int kind = 3;
#endif
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace
{
    constexpr uint32_t arena = 0x2ab080, counter = arena + 1690000u * 16,
        query = 0x1c74990, gp = 0x250000, stack = 0x260000;
    int failures = 0, cases = 0, initializers = 0, channels = 0;
    uint64_t savedQwords = 0;
    std::vector<std::pair<uint32_t,uint32_t>> allocations;
    RecompiledFunction allocate = nullptr, initialize = nullptr;
    void check(bool ok, const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL storage %d case %d: %s\n", kind, cases, what); }
    }
    void word(uint8_t *ram, uint32_t at, uint32_t v) { std::memcpy(ram + at, &v, 4); }
    uint32_t word(const uint8_t *ram, uint32_t at) { uint32_t v; std::memcpy(&v, ram + at, 4); return v; }
    void boundary(uint8_t *ram, R5900Context *ctx, Runtime *rt)
    {
        bozo::libs::Guest g(*rt, ctx);
        if (ctx->pc == 0x127a00)
        {
            const auto size = g.arg(1); allocate(ram, ctx, rt);
            allocations.emplace_back(size, ctx->r[2].uw[0]); return;
        }
        if (ctx->pc == 0x12c720) { ++initializers; initialize(ram, ctx, rt); return; }
        if (ctx->pc == 0x10c3a8) { ++channels; check(g.arg(0) == 1, "channel lookup input retained"); g.retU(0x10009000); }
        else if (ctx->pc == 0x104698) g.ret(0);
        else throw std::runtime_error("unexpected packet-storage boundary");
        ctx->pc = ctx->r[31].uw[0];
    }
    struct Host : bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &, R5900Context *, uint32_t) override { throw std::runtime_error("unexpected VU0"); }
        void ioRead(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &, R5900Context *, uint32_t, uint32_t, const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &, R5900Context *, const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(uint32_t packetSize, uint32_t readSize)
    {
        R5900Context ctx;
        for (int r = 16; r < 32; ++r)
        {
            ctx.r[r].ud[0] = 0x1234567800000000ull + r;
            ctx.r[r].ud[1] = 0xa5a55a5a00000000ull + r;
        }
        ctx.r[4].uw[0] = packetSize; ctx.r[5].uw[0] = readSize;
        ctx.r[28].ud[0] = gp; ctx.r[29].ud[0] = stack; ctx.r[31].ud[0] = 0x123450;
        return ctx;
    }
}
int main()
{
    Host host; bozo::ee::Dispatcher dispatcher(host); auto &rt = dispatcher.runtime();
    rt.functions.setGenerated(dc_function_table, dc_function_count);
    allocate = rt.functions.generated(0x127a00); initialize = rt.functions.generated(0x12c720);
    for (auto pc : {0x127a00u,0x12c720u,0x104698u,0x10c3a8u}) rt.functions.setOverride(pc, boundary);
    auto *ram = rt.memory.ram();
    const uint32_t sizes[] = {0,1,5,50000};
    for (unsigned edge = 0; edge < (kind == 1 ? 1u : 4u); ++edge)
        for (unsigned seed = 0; seed < (kind == 0 ? 4u : 1u); ++seed)
        {
            ++cases;
            std::memset(ram, 0, bozo::kFlatScratchpadOffset);
            std::memset(ram + arena, 0xa5, 1690000u * 16);
            word(ram, counter, seed); word(ram, 0x1c74980, seed);
            for (auto offset : {0x742cu,0x73f8u,0x73f4u}) word(ram, gp - offset, 0x5a5a5a5a);
            const uint32_t oldHandle = std::array<uint32_t,4>{0,0x1c7a780,0x12000000,0xffffffff}[edge];
            if (kind >= 2) word(ram, gp - 0x742c, oldHandle);
            std::memset(ram + stack - 0x1000, 0x5a, 0x1000);
            const std::vector<uint8_t> initial(ram, ram + bozo::kFlatScratchpadOffset);
            allocations.clear(); initializers = channels = 0;
            auto reference = context(sizes[edge], 17 + edge);
            REFERENCE[0].function(ram, &reference, &rt);
            check(reference.pc == 0x123450 && !reference.leaving, "reference returns normally");
            const auto originalAllocations = allocations;
            const auto originalCounter = word(ram, counter);
            if (kind < 2) check(initializers == 1 && allocations.size() == (kind == 0 ? 4 : 10), "reference reserves two packets and initializes once");
            if (kind == 2) check(channels == 1, "reference channel lookup retained");
            if (kind == 3) check(reference.r[2].uw[0] == oldHandle, "reference reads legacy global handle");
            std::vector<uint8_t> expected(ram, ram + initial.size());
            std::memcpy(ram, initial.data(), initial.size());
            auto native = context(sizes[edge], 17 + edge);
            if (kind == 3) native.r[28].uw[0] = 0x12000000;
            const auto before = native; allocations.clear(); initializers = channels = 0;
            ADAPTED[0].function(ram, &native, &rt);
            check(native.pc == 0x123450 && !native.leaving, "adapted returns normally");
            check(initializers == 0, "retired initializer never reached");
            if (kind < 2)
            {
                check(allocations.size() == originalAllocations.size() - 3, "only two packet and one tile reserves removed");
                const unsigned prefix = kind == 0 ? 1 : 7;
                for (unsigned i = 0; i < prefix; ++i) check(allocations.at(i) == originalAllocations.at(i), "live allocation inputs/addresses retained");
                if (kind == 0)
                {
                    check(word(ram, query + 12) == 0x800, "logical visibility extent retained");
                }
                check(word(ram, query) == 0, "no visibility tile backing pointer");
                word(expected.data(), query, 0);
                const auto savings = kind == 0 ? 2u * ((sizes[edge] + 3u) & ~3u) + 0x800u + ((4u - ((17u + edge) & 3u)) & 3u) : 104096u;
                check(originalCounter - word(ram, counter) == savings, "exact arena reservation reduction");
                savedQwords += savings; word(expected.data(), counter, originalCounter - savings);
                for (auto offset : {0x73f8u,0x73f4u}) word(expected.data(), gp - offset, 0x5a5a5a5a);
            }
            // The VIF1 handle is written as the constant sceDmaGetChan returned, without the lookup.
            if (kind == 2) { check(channels == 0, "adapted handle without a channel lookup"); word(expected.data(), gp - 0x742c, oldHandle); }
            if (kind == 3) check(native.r[2].ud[0] == 0, "getter returns null with inaccessible GP");
            check(std::memcmp(ram, expected.data(), stack - 0x1000) == 0 &&
                std::memcmp(ram + stack, expected.data() + stack, expected.size() - stack) == 0,
                "all other RAM matches, including full arena contents");
            for (int r : {16,17,18,19,20,21,22,23,28,29,30,31})
                if (!(kind == 2 && r == 23)) check(std::memcmp(&native.r[r], &before.r[r], sizeof(native.r[r])) == 0, "saved full registers preserved");
        }
    std::printf("DC packet storage %d: %d cases, %llu qwords no longer reserved, %d failures\n", kind, cases,
        static_cast<unsigned long long>(savedQwords), failures);
    return failures ? 1 : 0;
}
