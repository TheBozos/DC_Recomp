// Exact whole retail shadow wrappers, original/adapted. Vector/matrix copies
// and packet helpers execute compiled originals; native state/draw boundaries
// are controlled and their semantic inputs and logical effects are compared.
#include "dc_functions.h"
#if defined(DC_SHADOW_FAST)
#include "shadow_fast_ref_functions.h"
#include "shadow_fast_native_functions.h"
#define SHADOW_REFERENCE shadow_fast_ref_function_table
#define SHADOW_NATIVE shadow_fast_native_function_table
constexpr uint32_t shadowMode=1, originalChain=0x249900;
#elif defined(DC_SHADOW_FAST2)
#include "shadow_fast2_ref_functions.h"
#include "shadow_fast2_native_functions.h"
#define SHADOW_REFERENCE shadow_fast2_ref_function_table
#define SHADOW_NATIVE shadow_fast2_native_function_table
constexpr uint32_t shadowMode=1, originalChain=0x24dbf0;
#else
#include "shadow_ref_functions.h"
#include "shadow_native_functions.h"
#define SHADOW_REFERENCE shadow_ref_function_table
#define SHADOW_NATIVE shadow_native_function_table
constexpr uint32_t shadowMode=2, originalChain=0x24b670;
#endif
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr uint32_t gp=0x300000, stack=0x700000, packet=0x500000, output=0x510000,
        frame=0x400000, source=0x410000, marker=0x420000, matrices=0x1c75750, vectors=0x1c757d0, mode=0x1c75840;
    int failures=0, cases=0, packetCalls=0, stateCalls=0;
    uint64_t drawCalls=0;
    std::vector<BozoFunctionEntry> originals;
    struct Call
    {
        uint32_t pc=0, destination=0, frame=0, mode=0;
        std::array<uint8_t,160> data{};
        bool operator==(const Call &) const = default;
    };
    std::vector<Call> calls;
    void check(bool ok,const char *what)
    {
        if (!ok) { ++failures; std::printf("FAIL case %d: %s\n",cases,what); }
    }
    void word(uint8_t *ram,uint32_t at,uint32_t value) { std::memcpy(ram+at,&value,4); }
    void callback(uint8_t *ram,R5900Context *ctx,Runtime *rt)
    {
        bozo::libs::Guest g(*rt,ctx); const uint32_t pc=ctx->pc;
        if (pc==0x120d70)
        {
            ++packetCalls;
            check(g.arg(1)==(packetCalls==1 ? originalChain : 0x246100) && g.arg(2)==0,
                  "reference selects its original leading chain and default restore");
        }
        else if (pc==0x12edf0 || pc==0x12eec0)
        {
            // Published native setters serialize nothing and ignore their input.
            ++stateCalls; ctx->pc=ctx->r[31].uw[0]; return;
        }
        else if (pc==0x12ed80)
        {
            Call call{}; call.pc=pc; call.frame=g.arg(0); call.mode=g.read32(mode);
            check(call.mode==shadowMode && call.frame==frame,"draw sees the retained shadow mode and frame");
            std::memcpy(call.data.data(),g.in(matrices,160),160); calls.push_back(call); ++drawCalls;
            // A native frame draw may update camera matrices. The non-fast owner
            // must restore BOTH matrices, rather than merely matching an inert draw.
            for (uint32_t i=0;i<32;++i) g.write32(matrices+i*4,0x3f800000u+i*0x10001u);
            g.write32(marker,g.read32(marker)+1); g.retU(0x89abcdef); ctx->pc=ctx->r[31].uw[0]; return;
        }
        else if (pc==0x121830 || pc==0x121840)
        {
            Call call{}; call.pc=pc; call.destination=g.arg(0);
            const uint32_t bytes=pc==0x121830 ? 16 : 64;
            std::memcpy(call.data.data(),g.in(g.arg(1),bytes),bytes); calls.push_back(call);
        }
        else throw std::runtime_error("unexpected shadow callback");
        for (const auto &entry:originals) if (entry.address==pc) { entry.function(ram,ctx,rt); return; }
        throw std::runtime_error("missing compiled shadow helper");
    }
    struct Host:bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected VU0"); }
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(bool present,uint32_t a,uint32_t b)
    {
        R5900Context ctx;
        for (int r=16;r<=31;++r) { ctx.r[r].ud[0]=0x1234567800000000ull+r; ctx.r[r].ud[1]=0xa5a55a5a00000000ull+r; }
        ctx.r[4].uw[0]=present ? frame : 0; ctx.r[5].uw[0]=a; ctx.r[6].uw[0]=b;
        ctx.r[28].ud[0]=gp; ctx.r[29].ud[0]=stack; ctx.r[31].ud[0]=0x123450; ctx.fcr31=0x00800000;
        return ctx;
    }
    bool sameNonStack(const uint8_t *ram,const std::vector<uint8_t> &expected)
    {
        return std::memcmp(ram,expected.data(),stack-0x1000)==0 &&
            std::memcmp(ram+stack,expected.data()+stack,expected.size()-stack)==0;
    }
}
int main()
{
    Host host; bozo::ee::Dispatcher dispatcher(host); auto &rt=dispatcher.runtime(); auto *ram=rt.memory.ram();
    rt.functions.setGenerated(dc_function_table,dc_function_count);
    for (uint32_t pc:{0x121830u,0x121840u,0x120d70u})
        originals.push_back({pc,rt.functions.generated(pc)});
    for (uint32_t pc:{0x121830u,0x121840u,0x120d70u,0x12edf0u,0x12eec0u,0x12ed80u})
        rt.functions.setOverride(pc,callback);
    for (bool present:{false,true}) for (uint32_t aliases=0;aliases<4;++aliases)
        for (uint32_t pattern:{0u,1u,0x7fffffffu,0xffffffffu})
            for (uint32_t alignment=0;alignment<4;++alignment)
            {
                ++cases;
                for (uint32_t i=0;i<32;++i) word(ram,matrices+i*4,pattern^(0x3f800000u+i*0x12345u));
                for (uint32_t i=0;i<8;++i) word(ram,vectors+i*4,pattern^(0x80000000u+i*0x13579u));
                for (uint32_t i=0;i<8;++i) word(ram,source+i*4,pattern^(0x3f000000u+i*0x100001u));
                const uint32_t a=(aliases&1) ? vectors+16 : source;
                const uint32_t b=(aliases&2) ? vectors : source+16;
                word(ram,mode,pattern); word(ram,marker,0);
                std::memset(ram+packet,0,0x20000); word(ram,packet,output+32+alignment*4);
                word(ram,packet+4,output); word(ram,packet+8,alignment&1 ? output : 0);
                word(ram,gp-0x742c,packet); word(ram,gp-0x7410,pattern); word(ram,gp-0x740c,pattern^0xa5a5a5a5);
                word(ram,gp-0x7418,pattern); word(ram,gp-0x7414,pattern^0x5a5a5a5a);
                std::memset(ram+stack-0x1000,0xa5,0x1000);
                const std::vector<uint8_t> initial(ram,ram+bozo::kFlatScratchpadOffset);
                calls.clear(); packetCalls=stateCalls=0; auto reference=context(present,a,b);
                SHADOW_REFERENCE[0].function(ram,&reference,&rt);
                check(reference.pc==0x123450 && !reference.leaving,"reference returns normally");
                check(packetCalls==(present ? 2 : 0) && stateCalls==(present ? 2 : 0),"reference emits only non-null frame state/chains");
                const auto expectedCalls=calls;
                auto expected=initial;
                std::memcpy(expected.data()+matrices,ram+matrices,160);
                std::memcpy(expected.data()+mode,ram+mode,4); std::memcpy(expected.data()+marker,ram+marker,4);
                std::memcpy(expected.data()+packet,ram+packet,0x20000);
                check(sameNonStack(ram,expected),"reference touches only retained vectors/matrices/mode/draw effects and packet storage");
#if !defined(DC_SHADOW_FAST) && !defined(DC_SHADOW_FAST2)
                check(std::memcmp(ram+matrices,initial.data()+matrices,128)==0,"normal shadow restores both matrices after a mutating draw");
#endif
                std::memcpy(ram,initial.data(),initial.size()); word(ram,gp-0x742c,0x12000000);
                std::memset(ram+packet,0xa5,0x20000); word(expected.data(),gp-0x742c,0x12000000);
                std::memset(expected.data()+packet,0xa5,0x20000);
                calls.clear(); packetCalls=stateCalls=0; auto native=context(present,a,b); const auto before=native;
                SHADOW_NATIVE[0].function(ram,&native,&rt);
                check(native.pc==0x123450 && !native.leaving,"adapted returns normally");
                check(packetCalls==0 && stateCalls==0 && calls==expectedCalls,"retained copy/draw inputs and order match without graphics envelopes");
                check(sameNonStack(ram,expected),"all non-stack logical RAM matches, inaccessible packet pointer/guards untouched");
                for (int r:{16,17,18,19,20,21,22,23,28,29,30,31})
                    check(std::memcmp(&native.r[r],&before.r[r],sizeof(native.r[r]))==0,"full saved registers, stack and return preserved");
                check(native.fcr31==reference.fcr31,"floating-point status matches");
            }
    check(drawCalls>0,"fixtures execute the retained model draw");
    std::printf("DC shadow mode %u: %d original/adapted cases, %llu model draws, %d failures\n",shadowMode,cases,
        static_cast<unsigned long long>(drawCalls),failures);
    return failures ? 1 : 0;
}
