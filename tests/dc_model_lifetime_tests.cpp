// Static visuals own the model captured at construction, even when the source
// allocation is reused without changing its header. Explicit MDT visuals and
// Remake remain live update boundaries. Synthetic assets; no retail data.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    constexpr uint32_t visual=0x400000, asset=0x410000, matrix=0x420000, info=0x430000, data=0x12000000;
    int failures=0;
    void check(bool ok, const char *what) { if (!ok) { ++failures; std::printf("FAIL %s\n",what); } }
    void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
    void scalar(uint8_t *ram,uint32_t at,float v) { word(ram,at,std::bit_cast<uint32_t>(v)); }
    void identity(uint8_t *ram,uint32_t at)
    {
        for (unsigned i=0;i<16;++i) scalar(ram,at+i*4,i%5==0 ? 1.0f : 0.0f);
    }
    void assetData(uint8_t *ram)
    {
        std::memset(ram+asset,0,0x200);
        word(ram,asset,0x0054444d); word(ram,asset+8,0x200);
        word(ram,asset+0x10,0x40); word(ram,asset+0x18,0x70);
        word(ram,asset+0x28,0xd0); word(ram,asset+0x30,0xa0);
        const float positions[3][4]={{-0.5f,-0.5f,0,1},{0.5f,-0.5f,0,1},{0,0.5f,0,1}};
        std::memcpy(ram+asset+0x40,positions,sizeof(positions));
        for (unsigned i=0;i<3;++i) scalar(ram,asset+0x70+i*16+8,1);
        word(ram,asset+0xd8,1); // one triangle-list primitive
        word(ram,asset+0xe0,3); word(ram,asset+0xe4,3); word(ram,asset+0xe8,0xffffffff);
        for (unsigned i=0;i<3;++i)
            for (unsigned j=0;j<3;++j) word(ram,asset+0xec+i*12+j*4,i);
    }
    void build(Runtime &rt,bozo::port::Registry &registry,bool remake=false)
    {
        R5900Context ctx; ctx.pc=remake ? 0x135e50 : 0x135aa0;
        ctx.r[4].uw[0]=visual; ctx.r[5].uw[0]=data; ctx.r[6].uw[0]=asset;
        ctx.r[28].uw[0]=0x300000; ctx.r[31].uw[0]=0x123450;
        registry.find(ctx.pc)(rt.memory.ram(),&ctx,&rt);
        check(ctx.pc==0x123450,"builder returns without accessing obsolete output address");
    }
    bozo::render::FrameDraws draw(Runtime &rt,bozo::port::Registry &registry)
    {
        auto &renderer=bozo::render::Renderer::instance();
        renderer.takeFrameDraws(); renderer.beginFrame({});
        R5900Context ctx; ctx.pc=0x135000;
        ctx.r[4].uw[0]=visual; ctx.r[5].uw[0]=data; ctx.r[6].uw[0]=matrix;
        ctx.r[7].uw[0]=info; ctx.r[8].uw[0]=6; ctx.r[31].uw[0]=0x123450;
        registry.find(ctx.pc)(rt.memory.ram(),&ctx,&rt);
        check(ctx.pc==0x123450,"draw returns normally");
        renderer.endFrame(); return renderer.takeFrameDraws();
    }
}
int main()
{
    auto rt=std::make_unique<Runtime>(); bozo::port::Registry registry;
    dc::native::install(*rt,registry); auto *ram=rt->memory.ram();
    identity(ram,matrix); identity(ram,info+0x10); identity(ram,info+0x230); identity(ram,info+0x270);
    word(ram,info+0x310,2); scalar(ram,info+0x1a0,1);
    for (unsigned i=0;i<4;++i) scalar(ram,info+0x190+i*4,128);
    assetData(ram); word(ram,visual+8,0x2a11c0); build(*rt,registry);
    const auto original=draw(*rt,registry);
    check(original.batches==1 && original.vertices==3,"synthetic static model submits one triangle");
    word(ram,asset+0xd8,0); // body reused; all 64 header bytes remain valid
    const auto reused=draw(*rt,registry);
    check(reused.hash==original.hash && reused.vertices==3,"static terrain survives source-body reuse with identical header");
    assetData(ram); scalar(ram,asset+0x40,-0.25f);
    const auto changedSource=draw(*rt,registry);
    check(changedSource.hash==original.hash,"static visual does not adopt unrelated source geometry");
    build(*rt,registry,true);
    const auto remade=draw(*rt,registry);
    check(remade.vertices==3 && remade.hash!=original.hash,"explicit Remake refreshes the retained static geometry");
    std::memset(ram+asset,0,0x200);
    check(draw(*rt,registry).hash==remade.hash,"static visual also survives complete source release");
    assetData(ram); word(ram,visual+8,0x2a11a0); word(ram,visual+0x20,asset); build(*rt,registry);
    const auto animated=draw(*rt,registry);
    scalar(ram,asset+0x40,-0.25f);
    const auto updated=draw(*rt,registry);
    check(updated.vertices==3 && updated.hash!=animated.hash,"explicit MDT visual retains live vertex animation");
    // A work-name/base coincidence cannot supply previous scene pixels. This
    // case also runs in disc-free CI, independently of the GPU flame checks.
    constexpr uint32_t manager=0x1c75870,record=manager+0x10f8;
    word(ram,manager,0);word(ram,record,1024u<<16);word(ram,record+4,(4u<<16)|256u);
    std::strcpy(reinterpret_cast<char *>(ram+record+8),"work");
    const uint64_t work=0x1a40u|(uint64_t(10)<<26)|(uint64_t(8)<<30)|(uint64_t(1)<<34);
    std::memcpy(ram+record+0x28,&work,8);
    word(ram,asset+8,0x240);word(ram,asset+0x38,0x180);word(ram,asset+0xe8,0);
    for(unsigned i=0;i<4;++i)scalar(ram,asset+0x180+i*4,1);
    std::strcpy(reinterpret_cast<char *>(ram+asset+0x180+0x34),"work");
    build(*rt,registry);draw(*rt,registry);
    const auto unproduced=bozo::render::Renderer::instance().published();
    check(unproduced && unproduced->batches.size()==1 && unproduced->batches[0].material.fromFrame==0 &&
          !unproduced->batches[0].material.textured,"unproduced work image retains geometry without guessing previous-frame texture");
    std::printf("DC model lifetime: %d failures\n",failures);
    return failures ? 1 : 0;
}
