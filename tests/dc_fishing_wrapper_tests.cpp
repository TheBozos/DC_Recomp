// Whole FishLineDraw, original vs guarded native point boundary. Real compiled
// vector math and packet helpers; controlled projection/texture/model services.
#include "dc_functions.h"
#include "fishing_ref_functions.h"
#include "fishing_native_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/render/renderer.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace dc { bool nativeInstruction(uint8_t *, R5900Context *, Runtime *, uint32_t); }
namespace
{
    constexpr uint32_t gp=0x300000, stack=0x700000, packet=0x500000, output=0x510000, marker=0x430000;
    int failures=0, cases=0, packetCalls=0, projections=0, profile=0;
    uint32_t mask=0;
    uint64_t totalSegments=0, modelCalls=0;
    std::vector<BozoFunctionEntry> originals;
    struct Call { uint32_t pc=0; std::array<uint32_t,20> data{}; bool operator==(const Call &) const = default; };
    struct Point { uint32_t decision=0; std::array<uint32_t,3> xyz{}; bool operator==(const Point &) const = default; };
    std::vector<Call> calls;
    std::vector<Point> points;
    void check(bool ok,const char *what) { if (!ok) { ++failures; std::printf("FAIL case %d: %s\n",cases,what); } }
    void word(uint8_t *ram,uint32_t at,uint32_t value) { std::memcpy(ram+at,&value,4); }
    void number(uint8_t *ram,uint32_t at,float value) { word(ram,at,std::bit_cast<uint32_t>(value)); }
    void real(uint8_t *ram,R5900Context *ctx,Runtime *rt)
    {
        bozo::libs::Guest g(*rt,ctx); const uint32_t pc=ctx->pc;
        const bool isPacket=pc==0x120c80 || pc==0x120e38 || pc==0x120ec0 || pc==0x121028 || pc==0x120ed8 || pc==0x120e90;
        if (isPacket)
        {
            ++packetCalls;
            if (pc==0x121028 && (g.arg(1)==4 || g.arg(1)==12))
            {
                const uint64_t packed=ctx->r[6].ud[0];
                points.push_back({g.arg(1),{static_cast<uint32_t>(packed)&0xffffu,
                    static_cast<uint32_t>(packed>>16)&0xffffu,static_cast<uint32_t>(packed>>32)}});
            }
        }
        else
        {
            Call call{}; call.pc=pc;
            int vectors=pc==0x122ff0 ? 4 : pc==0x122fd0 ? 3 :
                (pc==0x121648 || pc==0x1217e8) ? 1 : 2;
            for (int i=0;i<vectors;++i) std::memcpy(call.data.data()+i*4,g.in(g.arg(i+1),16),16);
            if (pc==0x1217e8) call.data[16]=ctx->fu[12];
            calls.push_back(call);
        }
        for (const auto &e:originals) if (e.address==pc) { e.function(ram,ctx,rt); return; }
        throw std::runtime_error("missing fishing helper");
    }
    void boundary(uint8_t *,R5900Context *ctx,Runtime *rt)
    {
        bozo::libs::Guest g(*rt,ctx); const uint32_t pc=ctx->pc;
        if (pc==0x12e280) { ++packetCalls; g.retU(g.read32(gp-0x742c)); ctx->pc=ctx->r[31].uw[0]; return; }
        Call call{}; call.pc=pc;
        if (pc==0x133070) { call.data[0]=g.arg(0); call.data[1]=g.arg(2); }
        else if (pc==0x12e290)
        {
            std::memcpy(call.data.data(),g.in(g.arg(1),16),16); call.data[4]=g.arg(2);
            const uint32_t i=static_cast<uint32_t>(projections++);
            // Normal, repeated/zero-length, and boundary coordinate profiles.
            g.write32(g.arg(0),profile==2 ? 65535-i*16 : 28000+(profile==1 ? i/2 : i)*16);
            g.write32(g.arg(0)+4,31000+(profile==1 ? i/2 : i)*8); g.write32(g.arg(0)+8,0x100000+i*100);
            g.ret((mask&(1u<<i)) ? 1 : 0);
        }
        else if (pc==0x12a230 || pc==0x128560 || pc==0x127ee0 || pc==0x128180 || pc==0x12ed80)
        {
            call.data[0]=g.arg(0); ++modelCalls;
            if (pc==0x128560) std::memcpy(call.data.data()+1,g.in(g.arg(1),64),64);
            if (pc==0x127ee0) std::memcpy(call.data.data()+1,g.in(g.arg(1),16),16);
            if (pc==0x128180) call.data[1]=g.arg(1);
        }
        else throw std::runtime_error("unexpected fishing boundary");
        calls.push_back(call); g.write32(marker,g.read32(marker)+pc);
        if (pc!=0x12e290) g.ret(0);
        ctx->pc=ctx->r[31].uw[0];
    }
    struct NativePort:bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::libs::Libraries &libraries() override { return libs; }
        void frameEnd(Runtime *,R5900Context *) override { throw std::runtime_error("unexpected frame boundary"); }
        bool callLibrary(Runtime *,R5900Context *,uint32_t) override { return false; }
        bool nativeGraphics() const override { return true; }
        bool nativeInstruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc) override
        {
            check(pc==0x1ab290,"only the declared inline point boundary executes");
            Point p; p.decision=ctx->r[5].uw[0];
            std::memcpy(p.xyz.data(),ram+ctx->r[29].uw[0]+0x70,12); points.push_back(p);
            const auto before=*ctx;
            const bool result=dc::nativeInstruction(ram,ctx,rt,pc);
            check(std::memcmp(&before,ctx,sizeof(before))==0,"point submission preserves the complete register context");
            return result;
        }
    };
    struct Host:bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected VU0 program"); }
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
    };
    R5900Context context(uint32_t mode)
    {
        R5900Context ctx;
        for (int r=16;r<=31;++r) { ctx.r[r].ud[0]=0x1234567800000000ull+r; ctx.r[r].ud[1]=0xa5a55a5a00000000ull+r; }
        ctx.r[4].uw[0]=mode; ctx.r[28].ud[0]=gp; ctx.r[29].ud[0]=stack; ctx.r[31].ud[0]=0x123450;
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
    NativePort port; rt.port=&port;
    rt.functions.setGenerated(dc_function_table,dc_function_count);
    for (uint32_t pc:{0x120c80u,0x120e38u,0x120ec0u,0x121028u,0x120ed8u,0x120e90u,
        0x122ff0u,0x122fd0u,0x1217a0u,0x1217e8u,0x1217b8u,0x121648u,0x121600u})
    { originals.push_back({pc,rt.functions.generated(pc)}); rt.functions.setOverride(pc,real); }
    for (uint32_t pc:{0x12e280u,0x133070u,0x12e290u,0x12a230u,0x128560u,0x127ee0u,0x128180u,0x12ed80u})
        rt.functions.setOverride(pc,boundary);
    auto &renderer=bozo::render::Renderer::instance(); renderer.setEnabled(true);
    for (uint32_t mode:{0u,1u}) for (int enabled=0;enabled<4;++enabled) for (profile=0;profile<3;++profile)
        for (uint32_t accepted:{0u,0xffffffu,0xaaaaaau,0x555555u,0xfffffcu,0x800001u})
        {
            ++cases; mask=accepted;
            for (uint32_t i=0;i<88;++i)
            {
                number(ram,0x1d55e30+i*16,10.0f+i);
                number(ram,0x1d55e34+i*16,static_cast<float>(static_cast<int>(i%3)-1));
                number(ram,0x1d55e38+i*16,30.0f+i); number(ram,0x1d55e3c+i*16,1);
            }
            number(ram,gp-0x6cc8,0); word(ram,gp-0x7850,enabled&1);
            word(ram,gp-0x6ca8,(enabled&1) ? 0x440000 : 0); word(ram,gp-0x6cac,(enabled&2) ? 0x450000 : 0);
            word(ram,gp-0x6cb4,(enabled&1) ? 0x460000 : 0);
            number(ram,gp-0x7c90,1.0f/3); number(ram,gp-0x7c8c,1.0f/3);
            word(ram,gp-0x6ca4,2); word(ram,gp-0x6c9c,3); word(ram,marker,0);
            std::memset(ram+packet,0,0x20000); word(ram,packet,output); word(ram,packet+4,output);
            word(ram,gp-0x742c,packet); std::memset(ram+stack-0x1000,0xa5,0x1000);
            const std::vector<uint8_t> initial(ram,ram+bozo::kFlatScratchpadOffset);
            calls.clear(); points.clear(); projections=packetCalls=0;
            auto original=context(mode); fishing_ref_function_table[0].function(ram,&original,&rt);
            check(original.pc==0x123450 && !original.leaving,"reference returns normally");
            check(projections==24 && points.size()==24 && packetCalls>=35,"reference visits all points and real packet helpers");
            const auto expectedCalls=calls; const auto expectedPoints=points;
            auto expected=initial;
            std::memcpy(expected.data()+packet,ram+packet,0x20000); std::memcpy(expected.data()+marker,ram+marker,4);
            check(sameNonStack(ram,expected),"reference changes only packets and controlled logical effects");
            std::memcpy(ram,initial.data(),initial.size()); word(ram,gp-0x742c,0x12000000);
            std::memset(ram+packet,0xa5,0x20000); std::memset(expected.data()+packet,0xa5,0x20000);
            word(expected.data(),gp-0x742c,0x12000000);
            calls.clear(); points.clear(); projections=packetCalls=0; renderer.beginFrame({}); renderer.takeFrameDraws();
            auto native=context(mode); const auto before=native;
            fishing_native_function_table[0].function(ram,&native,&rt); renderer.endFrame();
            check(native.pc==0x123450 && !native.leaving,"adapted returns normally");
            check(packetCalls==0 && calls==expectedCalls,"same ordered projection/vector/texture/model calls without packets");
            check(points==expectedPoints,"all unpacked coordinates and connection decisions match original output");
            check(sameNonStack(ram,expected),"same non-stack logical RAM, inaccessible packet pointer and guards untouched");
            for (int r:{16,17,18,19,20,21,22,23,28,29,30,31})
                check(std::memcmp(&native.r[r],&before.r[r],sizeof(native.r[r]))==0,"full saved registers and return preserved");
            check(native.fcr31==original.fcr31,"retained floating-point status matches");
            const auto frame=renderer.published(); size_t segment=0;
            for (size_t i=1;i<expectedPoints.size();++i) if (expectedPoints[i].decision==4)
            {
                if (expectedPoints[i-1].xyz[0]==expectedPoints[i].xyz[0] &&
                    expectedPoints[i-1].xyz[1]==expectedPoints[i].xyz[1]) continue;
                check(frame && segment<frame->batches.size(),"accepted connection submits native geometry");
                if (!frame || segment>=frame->batches.size()) break;
                const auto &batch=frame->batches[segment++]; const auto *v=frame->vertices.data()+batch.first;
                check(batch.count==6 && !batch.material.textured && batch.material.depthTest && !batch.material.depthWrite,
                    "native segment is six untextured depth-tested vertices");
                if (batch.count!=6) continue;
                for (int end=0;end<2;++end)
                {
                    const auto &p=expectedPoints[i-1+end]; const size_t a=end ? 2 : 0,b=end ? 4 : 1;
                    check(std::abs((v[a].x+v[b].x)*0.5f-p.xyz[0]/16.0f)<0.001f &&
                          std::abs((v[a].y+v[b].y)*0.5f-p.xyz[1]/16.0f)<0.001f && v[a].z==p.xyz[2],
                          "native segment centers and endpoint depths match original projections");
                    check(std::abs(std::hypot(v[a].x-v[b].x,v[a].y-v[b].y)-1.0f)<0.001f,
                          "native segment width is one logical pixel");
                }
            }
            check(frame && frame->batches.size()==segment,"rejected connections emit no geometry or cross-call joins");
            totalSegments+=segment;
        }
    check(totalSegments>0 && modelCalls>0,"fixtures exercise native line and retained pole/model paths");
    std::printf("DC fishing line: %d original/adapted cases, %llu native segments, %d failures\n",cases,
        static_cast<unsigned long long>(totalSegments),failures);
    return failures ? 1 : 0;
}
