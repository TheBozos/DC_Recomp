// Game-free native font composition: two calls interleaved across saved CPU
// contexts, immutable published images, and explicit unsupported-input guards.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace dc { bool nativeInstruction(uint8_t *,R5900Context *,Runtime *,uint32_t); }
namespace dc::native { void install(Runtime &,bozo::port::Registry &); }
namespace
{
    constexpr uint32_t atlas=0x410000, a=0x420000, b=0x430000, fontA=0x440000, fontB=0x450000,
        pixels=0x500000, pictureA=0x530000, pictureB=0x540000, rect=0x550000;
    int failures=0;
    void check(bool ok,const char *s) { if (!ok) { ++failures; std::printf("FAIL %s\n",s); } }
    void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
    void record(uint8_t *ram,uint32_t at,uint32_t data,uint32_t w,uint32_t h,uint32_t base)
    {
        word(ram,at,w<<16); word(ram,at+4,h); word(ram,at+0x38,data); word(ram,at+0x3c,w*h*4);
        const uint64_t tex0=base | (uint64_t(w/64)<<14) | (uint64_t(w==128 ? 7 : 6)<<26) | (uint64_t(h==256 ? 8 : 6)<<30);
        std::memcpy(ram+at+0x28,&tex0,8);
    }
    R5900Context context(uint32_t font,uint32_t stack)
    {
        R5900Context ctx; ctx.r[21].uw[0]=font; ctx.r[22].uw[0]=16; ctx.r[16].uw[0]=16;
        ctx.r[29].uw[0]=stack; ctx.r[28].uw[0]=0x300000; ctx.r[17].uw[0]=font+28; return ctx;
    }
    void boundary(Runtime &rt,R5900Context &ctx,uint32_t pc)
    {
        const auto before=ctx;
        check(dc::nativeInstruction(rt.memory.ram(),&ctx,&rt,pc),"native font site is handled");
        check(std::memcmp(&ctx,&before,sizeof(ctx))==0,"complete CPU context survives a pixel boundary");
    }
    void start(Runtime &rt,R5900Context &ctx,uint32_t destination)
    {
        ctx.r[2].uw[0]=atlas; boundary(rt,ctx,0x13dfa8);
        ctx.r[2].uw[0]=destination; boundary(rt,ctx,0x13dff0);
    }
    void draw(Runtime &rt,bozo::port::Registry &registry,uint32_t destination)
    {
        R5900Context ctx; ctx.r[5].uw[0]=destination; ctx.r[6].uw[0]=rect; ctx.r[7].uw[0]=rect;
        ctx.r[8].uw[0]=64; ctx.r[31].uw[0]=0x123450;
        registry.find(0x15c310)(rt.memory.ram(),&ctx,&rt);
    }
    void frameTexture(uint8_t *ram,R5900Context *ctx,Runtime *)
    {
        const uint64_t descriptor=0x2000; std::memcpy(ram+ctx->r[4].uw[0],&descriptor,8);
        ctx->pc=ctx->r[31].uw[0];
    }
}
int main()
{
    auto rt=std::make_unique<Runtime>(); auto *ram=rt->memory.ram(); bozo::port::Registry registry;
    const BozoFunctionEntry generated[]={{0x12f130,frameTexture},{0x12f1a0,frameTexture}};
    rt->functions.setGenerated(generated,2);
    dc::native::install(*rt,registry); auto &renderer=bozo::render::Renderer::instance();
    record(ram,atlas,pixels,128,256,0x400); record(ram,a,pictureA,64,64,0x2000); record(ram,b,pictureB,64,64,0x2200);
    // A generated font picture can have no CPU pixels and reuse an old frame
    // identifier. Its native content must beat that stale target association.
    word(ram,a+0x38,0);
    R5900Context frameCtx; frameCtx.r[4].uw[0]=rect+0x40; frameCtx.r[31].uw[0]=0x123450;
    registry.find(0x12f130)(ram,&frameCtx,rt.get());
    for (uint32_t y=0;y<256;++y) for (uint32_t x=0;x<128;++x) word(ram,pixels+(y*128+x)*4,0x80000000u|x|(y<<8));
    word(ram,fontA+20,128); word(ram,fontB+20,64); word(ram,fontA+24,42); word(ram,fontB+24,43);
    word(ram,fontA+28,'A'); word(ram,fontB+28,'B'); word(ram,rect+8,16); word(ram,rect+12,16);
    auto first=context(fontA,0x600000),second=context(fontB,0x610000);
    first.r[2].uw[0]=atlas; boundary(*rt,first,0x13dfa8);
    second.r[2].uw[0]=atlas; boundary(*rt,second,0x13dfa8);
    first.r[2].uw[0]=a; boundary(*rt,first,0x13dff0);
    second.r[2].uw[0]=b; boundary(*rt,second,0x13dff0);
    // Copies of the contexts model resumed calls with no outer C++ draw scope.
    auto resumedFirst=first,resumedSecond=second;
    boundary(*rt,resumedFirst,0x13e314); boundary(*rt,resumedSecond,0x13e314);
    boundary(*rt,resumedFirst,0x13e420); boundary(*rt,resumedSecond,0x13e420);
    renderer.beginFrame({}); draw(*rt,registry,a); draw(*rt,registry,b); renderer.endFrame();
    auto frame=renderer.published(); check(frame && frame->batches.size()==2,"two independent pictures are published");
    std::shared_ptr<const bozo::render::Texture> old; std::vector<uint32_t> oldPixels;
    if (frame && frame->batches.size()==2)
    {
        const auto one=frame->batches[0].texture,two=frame->batches[1].texture;
        check(one && two && one->rgba[0]==0x80002000 && two->rgba[0]==0x80002008,"interleaved calls retain distinct glyphs");
        check(one && one->rgba[15]==0x80010101,"clear extends beyond the glyph");
        old=one; if (old) oldPixels=old->rgba;
    }
    const auto ramAfter=std::vector<uint8_t>(ram,ram+bozo::kFlatScratchpadOffset);
    auto repeated=context(fontA,0x600000); word(ram,fontA+28,'C'); start(*rt,repeated,a);
    boundary(*rt,repeated,0x13e314); boundary(*rt,repeated,0x13e420);
    check(old && old->rgba==oldPixels,"recomposition never mutates a picture retained by a published frame");
    auto expected=ramAfter; word(expected.data(),fontA+28,'C'); expected[0x300000-0x7400]=128;
    check(std::memcmp(ram,expected.data(),expected.size())==0,"native composition changes no pixel RAM or packet memory");
    auto rebound=context(fontA,0x600000); start(*rt,rebound,a); boundary(*rt,rebound,0x13e314);
    // The record now names a different asset before the suspended call finishes.
    // Publishing the old font under that NEW identity would corrupt its pixels.
    record(ram,a,pictureA,64,64,0x2400);
    for (uint32_t i=0;i<64*64;++i) word(ram,pictureA+i*4,0x80ccbbaa);
    boundary(*rt,rebound,0x13e420);
    renderer.beginFrame({}); draw(*rt,registry,a); renderer.endFrame(); frame=renderer.published();
    check(frame && frame->batches.size()==1 && frame->batches[0].texture && frame->batches[0].texture->rgba[0]==0x80ccbbaa,
        "a rebound record never acquires the old composition under its new identity");
    bool failed=false; auto missing=context(fontA,0x620000);
    try { boundary(*rt,missing,0x13dff0); } catch (const std::runtime_error &) { failed=true; }
    check(failed,"resuming without owned composition fails explicitly");
    auto invalid=context(fontA,0x600000); invalid.r[2].uw[0]=atlas; boundary(*rt,invalid,0x13dfa8);
    invalid.r[2].uw[0]=a; invalid.r[22].uw[0]=65; failed=false;
    try { boundary(*rt,invalid,0x13dff0); } catch (const std::runtime_error &) { failed=true; }
    check(failed,"oversized picture fails explicitly");
    invalid=context(fontA,0x600000); invalid.r[2].uw[0]=atlas; boundary(*rt,invalid,0x13dfa8);
    invalid.r[2].uw[0]=atlas; failed=false;
    try { boundary(*rt,invalid,0x13dff0); } catch (const std::runtime_error &) { failed=true; }
    check(failed,"atlas/destination alias fails instead of interpreting shared graphics memory");
    invalid=context(fontA,0x600000); invalid.r[2].uw[0]=atlas; boundary(*rt,invalid,0x13dfa8);
    uint64_t descriptor; std::memcpy(&descriptor,ram+a+0x28,8); descriptor|=uint64_t(0x13)<<20;
    std::memcpy(ram+a+0x28,&descriptor,8); invalid.r[2].uw[0]=a; failed=false;
    try { boundary(*rt,invalid,0x13dff0); } catch (const std::runtime_error &) { failed=true; }
    check(failed,"indexed destination fails explicitly");
    std::memcpy(&descriptor,ram+atlas+0x28,8); descriptor|=uint64_t(0x13)<<20;
    std::memcpy(ram+atlas+0x28,&descriptor,8); invalid.r[2].uw[0]=atlas; failed=false;
    try { boundary(*rt,invalid,0x13dfa8); } catch (const std::runtime_error &) { failed=true; }
    check(failed,"indexed atlas fails explicitly");
    std::printf("DC native debug font: %d failures\n",failures); return failures ? 1 : 0;
}
