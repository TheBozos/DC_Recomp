// Complete original/adapted CDebugFont::Draw. The reference executes the real
// compiled append helper; its copy arguments form the independent pixel oracle.
// Texture lookup and final sprite are controlled boundaries; the adapted sprite
// also executes the actual native renderer hook and publishes the composed image.
#include "dc_functions.h"
#include "debug_font_ref_functions.h"
#include "debug_font_native_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/render/renderer.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
namespace dc { bool nativeInstruction(uint8_t *,R5900Context *,Runtime *,uint32_t); }
namespace dc::native { void install(Runtime &,bozo::port::Registry &); }
namespace
{
    constexpr uint32_t gp=0x300000, stack=0x700000, font=0x400000, atlas=0x410000, picture=0x420000,
        pixels=0x600000, destinationPixels=0x620000, packet=0x500000, output=0x510000, name=0x480000;
    int failures=0,cases=0,lookupCalls=0,packetCalls=0,envelopes=0,frameQueries=0,nativeCalls=0;
    bool adapted=false;
    uint64_t position=0; uint64_t copies=0;
    RecompiledFunction append=nullptr,sprite=nullptr;
    struct Copy { uint32_t sx,sy,dx,dy; };
    std::vector<Copy> rectangles;
    struct Call { uint32_t pc=0; std::array<uint32_t,10> data{}; bool operator==(const Call &) const=default; };
    std::vector<Call> calls;
    void check(bool ok,const char *s) { if (!ok) { ++failures; std::printf("FAIL case %d: %s\n",cases,s); } }
    void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
    uint32_t rgb(uint32_t x,uint32_t y) { return (x+y)%13==0 ? 0 : (1+x) | ((1+y)<<8) | (((x*7+y*3)&255)<<16); }
    void record(uint8_t *ram,uint32_t at,uint32_t data,uint32_t w,uint32_t h,uint32_t format,uint32_t base)
    {
        std::memset(ram+at,0,0x50); word(ram,at,w<<16); word(ram,at+4,h);
        const uint64_t tex0=base | (uint64_t(w/64)<<14) | (uint64_t(format)<<20) |
            (uint64_t(w==128 ? 7 : 6)<<26) | (uint64_t(h==256 ? 8 : 6)<<30);
        std::memcpy(ram+at+0x28,&tex0,8); word(ram,at+0x38,data); word(ram,at+0x3c,w*h*(format ? 3 : 4));
    }
    void callback(uint8_t *ram,R5900Context *ctx,Runtime *rt)
    {
        bozo::libs::Guest g(*rt,ctx); const uint32_t pc=ctx->pc;
        if (pc==0x121028)
        {
            ++packetCalls; const uint32_t reg=g.arg(1); const uint64_t value=ctx->r[6].ud[0];
            if (reg==0x51) position=value;
            if (reg==0x52) check(value==(uint64_t(16)<<32|8),"reference glyph is an 8x16 copy");
            if (reg==0x53)
            {
                check(value==2,"reference performs a local picture copy");
                rectangles.push_back({uint32_t(position)&0xffffu,uint32_t(position>>16)&0xffffu,
                    uint32_t(position>>32)&0xffffu,uint32_t(position>>48)&0xffffu});
            }
            append(ram,ctx,rt); return;
        }
        if (pc==0x13dea0 || pc==0x13df00) { ++envelopes; ctx->pc=ctx->r[31].uw[0]; return; }
        if (pc==0x12f130)
        {
            ++frameQueries; const uint64_t descriptor=0x6400000a00000000ull;
            g.write(g.arg(0),&descriptor,8);
            Call call{}; call.pc=pc; calls.push_back(call);
            ctx->pc=ctx->r[31].uw[0]; return;
        }
        Call call{}; call.pc=pc;
        if (pc==0x1312d0)
        {
            for (int i=0;i<3;++i) call.data[i]=g.arg(i);
            check(g.arg(0)==0x1c75870 && g.arg(1)==(lookupCalls==0 ? 0x299068 : name) && g.arg(2)==0xffffffffu,
                  "same original atlas/destination lookup contract");
            g.retU(lookupCalls++==0 ? atlas : picture);
        }
        else if (pc==0x15c310)
        {
            call.data[0]=g.arg(1); std::memcpy(call.data.data()+1,g.in(g.arg(2),16),16);
            std::memcpy(call.data.data()+5,g.in(g.arg(3),16),16); call.data[9]=g.arg(4);
            calls.push_back(call);
            if (adapted) { sprite(ram,ctx,rt); return; }
        }
        else throw std::runtime_error("unexpected debug font boundary");
        if (pc!=0x15c310) calls.push_back(call);
        ctx->pc=ctx->r[31].uw[0];
    }
    struct Host:bozo::ee::Host
    {
        void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
        void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected VU0"); }
        void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected device read"); }
        void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected device write"); }
        void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
    };
    struct Port:bozo::port::Host
    {
        bozo::libs::Libraries libs;
        bozo::libs::Libraries &libraries() override { return libs; }
        bool callLibrary(Runtime *,R5900Context *,uint32_t) override { return false; }
        bool nativeGraphics() const override { return true; }
        void frameEnd(Runtime *,R5900Context *) override { throw std::runtime_error("unexpected frame boundary"); }
        bool nativeInstruction(uint8_t *ram,R5900Context *ctx,Runtime *rt,uint32_t pc) override
        {
            ++nativeCalls; const auto before=*ctx; const bool result=dc::nativeInstruction(ram,ctx,rt,pc);
            check(std::memcmp(ctx,&before,sizeof(before))==0,"native pixel boundary preserves the entire CPU context");
            return result;
        }
    };
    R5900Context context()
    {
        R5900Context ctx;
        for (int r=16;r<=31;++r) { ctx.r[r].ud[0]=0x1234567800000000ull+r; ctx.r[r].ud[1]=0xa5a55a5a00000000ull+r; }
        ctx.r[4].uw[0]=font; ctx.r[28].ud[0]=gp; ctx.r[29].ud[0]=stack; ctx.r[31].ud[0]=0x123450;
        ctx.fcr31=0x00800000; return ctx;
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
    Port port; rt.port=&port; rt.functions.setGenerated(dc_function_table,dc_function_count);
    bozo::port::Registry registry; dc::native::install(rt,registry); sprite=registry.find(0x15c310);
    append=rt.functions.generated(0x121028);
    for (uint32_t pc:{0x121028u,0x13dea0u,0x13df00u,0x12f130u,0x1312d0u,0x15c310u}) rt.functions.setOverride(pc,callback);
    auto &renderer=bozo::render::Renderer::instance();
    const std::array<std::string,8> texts={"","A"," A B ","A\nB\n C","\n\nA","0123456789abcdefghi",
        std::string("!\x01\x1f\x7f\x80\xff",6),"A B\nC D\nEFG\nHIJ\nK"};
    std::shared_ptr<const bozo::render::Texture> oldImage; std::vector<uint32_t> oldPixels;
    for (uint32_t srcFormat:{0u,1u}) for (uint32_t dstFormat:{0u,1u}) for (uint32_t alpha:{0u,64u,128u})
        for (const auto &text:texts) for (bool small:{false,true})
        {
            ++cases; const uint32_t width=small ? 31 : 64,height=small ? 17 : 32;
            record(ram,atlas,pixels,128,256,srcFormat,0x400);
            record(ram,picture,destinationPixels,64,64,dstFormat,0x2000+cases*32);
            for (uint32_t y=0;y<256;++y) for (uint32_t x=0;x<128;++x)
            {
                const uint32_t value=rgb(x,y)|0x55000000u,at=pixels+(y*128+x)*(srcFormat ? 3 : 4);
                std::memcpy(ram+at,&value,srcFormat ? 3 : 4);
            }
            for (uint32_t i=0;i<64*64;++i)
            { const uint32_t value=0x55050607; std::memcpy(ram+destinationPixels+i*(dstFormat ? 3 : 4),&value,dstFormat ? 3 : 4); }
            word(ram,font,0xfffffff7); word(ram,font+4,13); word(ram,font+8,width); word(ram,font+12,height);
            word(ram,font+16,name); word(ram,font+20,alpha); word(ram,font+24,123);
            std::memcpy(ram+font+28,text.c_str(),text.size()+1);
            word(ram,gp-0x7400,0x12345678); word(ram,gp-0x73fc,0x87654321);
            std::memset(ram+packet,0,0x20000); word(ram,packet,output); word(ram,packet+4,output);
            word(ram,gp-0x742c,packet); std::memset(ram+stack-0x1000,0xa5,0x1000);
            const std::vector<uint8_t> initial(ram,ram+bozo::kFlatScratchpadOffset);
            calls.clear(); rectangles.clear(); packetCalls=envelopes=frameQueries=lookupCalls=nativeCalls=0; adapted=false;
            auto reference=context(); debug_font_ref_function_table[0].function(ram,&reference,&rt);
            check(reference.pc==0x123450 && !reference.leaving,"reference returns normally");
            check(lookupCalls==3 && envelopes==2 && frameQueries==1,"reference retains three lookups and old state envelope");
            const auto expectedCalls=calls; const auto expectedCopies=rectangles; copies+=rectangles.size();
            auto expected=initial; word(expected.data(),font+24,0);
            std::memcpy(expected.data()+gp-0x7400,ram+gp-0x7400,8); std::memcpy(expected.data()+packet,ram+packet,0x20000);
            check(sameNonStack(ram,expected),"reference changes only count/TEXA fields and packet output");
            std::vector<uint32_t> expectedImage(64*64,(dstFormat ? alpha<<24 : 0x55000000u)|0x050607u);
            for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x)
                expectedImage[y*64+x]=(dstFormat ? alpha<<24 : 0x80000000u)|0x010101u;
            for (const auto &r:expectedCopies) for (uint32_t y=0;y<16;++y) for (uint32_t x=0;x<8;++x)
                if (r.dx+x<64 && r.dy+y<64)
                {
                    const uint32_t color=rgb(r.sx+x,r.sy+y); auto &pixel=expectedImage[(r.dy+y)*64+r.dx+x];
                    pixel=color | (dstFormat ? (color ? alpha<<24 : 0u) : pixel&0xff000000u);
                }
            std::memcpy(ram,initial.data(),initial.size()); word(ram,gp-0x742c,0x12000000);
            std::memset(ram+packet,0xa5,0x20000); word(expected.data(),gp-0x742c,0x12000000);
            std::memset(expected.data()+packet,0xa5,0x20000);
            calls.clear(); rectangles.clear(); packetCalls=envelopes=frameQueries=lookupCalls=nativeCalls=0; adapted=true;
            renderer.beginFrame({}); auto native=context(); const auto before=native;
            debug_font_native_function_table[0].function(ram,&native,&rt); renderer.endFrame();
            check(native.pc==0x123450 && !native.leaving,"adapted returns normally");
            check(lookupCalls==3 && packetCalls==0 && envelopes==0 && frameQueries==1 && calls==expectedCalls,
                  "same texture/frame-query/final-sprite semantics without packets or descriptor packing");
            check(nativeCalls==3+int(expectedCopies.size()),"only source/begin/glyph/finish native boundaries execute");
            check(sameNonStack(ram,expected),"logical RAM matches and inaccessible packet/guards remain untouched");
            for (int r:{16,17,18,19,20,21,22,23,28,29,30,31})
                check(std::memcmp(&native.r[r],&before.r[r],sizeof(native.r[r]))==0,"full saved registers/stack/return preserved");
            check(native.fcr31==reference.fcr31,"outgoing floating-point status matches");
            const auto frame=renderer.published();
            check(frame && frame->batches.size()==1 && frame->batches[0].texture,"actual final sprite publishes the native picture");
            if (frame && frame->batches.size()==1 && frame->batches[0].texture)
            {
                const auto image=frame->batches[0].texture;
                check(image->width==64 && image->height==64 && image->rgba==expectedImage,"every composed native texel matches reference copy rectangles");
                if (oldImage) check(oldImage->rgba==oldPixels,"a subsequent call cannot mutate a published texture");
                oldImage=image; oldPixels=image->rgba;
            }
        }
    std::printf("DC debug font: %d original/adapted cases, %llu reference glyph copies, %d failures\n",cases,
        static_cast<unsigned long long>(copies),failures); return failures ? 1 : 0;
}
