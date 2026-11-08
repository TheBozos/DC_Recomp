// Static visuals own the model captured at construction, even when the source
// allocation is reused without changing its header. Explicit MDT visuals and
// Remake remain live update boundaries. Synthetic assets; no retail data.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <bit>
#include <chrono>
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
    void compactTextures(uint8_t *ram,R5900Context *ctx,Runtime *)
    {
        const uint32_t first=ctx->r[4].uw[0]+0x10f8;
        for(unsigned hole=0;hole<195;++hole)if(!ram[first+hole*80+8])
            for(unsigned from=hole+1;from<196;++from)if(ram[first+from*80+8])
            {std::memcpy(ram+first+hole*80,ram+first+from*80,80);std::memset(ram+first+from*80,0,80);break;}
        ctx->r[2].ud[0]=1;ctx->pc=ctx->r[31].uw[0];
    }
    void parsedTexture(uint8_t *,R5900Context *ctx,Runtime *){ctx->pc=ctx->r[31].uw[0];}
    void frameTexture(uint8_t *ram,R5900Context *ctx,Runtime *)
    {word(ram,ctx->r[4].uw[0],0x1000);word(ram,ctx->r[4].uw[0]+4,0);ctx->pc=ctx->r[31].uw[0];}
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
int main(int argc,char **argv)
{
    auto rt=std::make_unique<Runtime>(); bozo::port::Registry registry;
    const BozoFunctionEntry originals[]={{0x12f130,frameTexture},{0x12f1a0,frameTexture},{0x133a60,compactTextures},{0x134760,parsedTexture}};
    rt->functions.setGenerated(originals,4);
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
    // A real work producer still does not grant a missing material ownership of
    // that image. Norune's generic sky gradient names an absent texture; the
    // allocation ABI's default record zero must not become a render binding.
    constexpr uint32_t sourceTex=0x480000,destTex=sourceTex+16,copyRect=sourceTex+32;
    R5900Context getFrame;getFrame.pc=0x12f130;getFrame.r[4].uw[0]=sourceTex;getFrame.r[31].uw[0]=0x123450;
    check(registry.find(getFrame.pc)!=nullptr,"frame producer SDK pair is installed");
    if(!registry.find(getFrame.pc))return 1;
    registry.find(getFrame.pc)(ram,&getFrame,rt.get());
    std::memcpy(ram+destTex,&work,8);word(ram,copyRect+8,640);word(ram,copyRect+12,224);
    R5900Context copy;copy.pc=0x12f210;copy.r[4].uw[0]=sourceTex;copy.r[5].uw[0]=copyRect;
    copy.r[6].uw[0]=destTex;copy.r[31].uw[0]=0x123450;
    auto &renderer=bozo::render::Renderer::instance();renderer.beginFrame({1728,1936,640,224});
    registry.find(copy.pc)(ram,&copy,rt.get());renderer.endFrame();
    check(copy.pc==0x123450,"work image producer returns normally");
    std::strcpy(reinterpret_cast<char *>(ram+asset+0x180+0x34),"missing_sky");
    build(*rt,registry);draw(*rt,registry);
    const auto missingSky=renderer.published();
    check(missingSky && missingSky->batches.size()==1 && !missingSky->batches[0].material.fromFrame &&
          !missingSky->batches[0].material.textured,"missing sky material cannot sample a different owner's produced work image");
    std::strcpy(reinterpret_cast<char *>(ram+asset+0x180+0x34),"work");
    build(*rt,registry);draw(*rt,registry);const auto explicitWork=renderer.published();
    check(explicitWork && explicitWork->batches.size()==1 && explicitWork->batches[0].material.fromFrame &&
          explicitWork->batches[0].material.textured,"explicitly bound work material retains its native produced image");
    // Texture table storage is compacted/reused by menu and equipment loads.
    // Retain the bound asset, while still following its live animation updates.
    constexpr uint32_t floor=record+0x50, moved=floor+0x50, duplicate=moved+0x50,
        floorPixels=0x450000,menuPixels=0x460000,otherPixels=0x470000;
    auto picture=[&](uint32_t at,const char *name,uint32_t pixels,uint32_t colour) {
        std::memset(ram+at,0,0x50);word(ram,at,(2u<<16)|13);word(ram,at+4,(4u<<16)|2);
        std::strcpy(reinterpret_cast<char *>(ram+at+8),name);
        const uint64_t tex0=0x500u|(uint64_t(1)<<26)|(uint64_t(1)<<30)|(uint64_t(1)<<34);
        std::memcpy(ram+at+0x28,&tex0,8);word(ram,at+0x38,pixels);word(ram,at+0x3c,16);
        for(int i=0;i<4;++i)word(ram,pixels+i*4,colour);
    };
    auto firstTexel=[&]() {
        draw(*rt,registry);auto f=bozo::render::Renderer::instance().published();
        check(f && f->batches.size()==1 && f->batches[0].texture,"bound texture remains drawable");
        return f && !f->batches.empty() && f->batches[0].texture ? f->batches[0].texture->rgba[0] : 0u;
    };
    assetData(ram);word(ram,visual+8,0x2a11c0);word(ram,visual+0x20,0);
    word(ram,asset+8,0x240);word(ram,asset+0x38,0x180);word(ram,asset+0xe8,0);
    for(unsigned i=0;i<4;++i)scalar(ram,asset+0x180+i*4,1);
    std::strcpy(reinterpret_cast<char *>(ram+asset+0x180+0x34),"floor");
    word(ram,manager,2);picture(floor,"floor",floorPixels,0x80000080);build(*rt,registry);
    const uint32_t red=firstTexel();check(red==0x80000080,"floor starts red");
    std::memcpy(ram+moved,ram+floor,0x50);word(ram,manager,3);
    picture(floor,"menu",menuPixels,0x80008000);
    check(firstTexel()==red,"menu reusing the old table slot cannot replace the dungeon texture");
    picture(duplicate,"floor",otherPixels,0x80800000);word(ram,manager,4);
    check(firstTexel()==red,"same name and TEX0 in a newer asset do not steal an existing binding");
    build(*rt,registry,true);check(firstTexel()==red,"Remake preserves the original texture identity after compaction");
    for(int i=0;i<4;++i)word(ram,floorPixels+i*4,0x80008080);
    const uint32_t yellow=firstTexel();check(yellow==0x80008080,"relocated live texel updates remain visible");
    std::memset(ram+moved,0,0x50);
    check(firstTexel()==yellow,"released records keep their last owned image without selecting a name collision");
    build(*rt,registry);const uint32_t blue=firstTexel();
    check(blue==0x80800000,"explicit fresh construction binds the current newest asset");
    // A full-size image is still checked on each lookup. Neither a cached
    // binding nor a frame-local shortcut may hide in-place edits near its end.
    word(ram,duplicate,(256u<<16)|13);word(ram,duplicate+4,(4u<<16)|256u);
    const uint64_t largeTex0=0x500u|(uint64_t(8)<<26)|(uint64_t(8)<<30)|(uint64_t(1)<<34);
    std::memcpy(ram+duplicate+0x28,&largeTex0,8);
    for(unsigned i=0;i<256*256;++i)word(ram,otherPixels+i*4,0x80000080);
    build(*rt,registry);draw(*rt,registry);
    const auto beforeEdit=bozo::render::Renderer::instance().published()->batches[0].texture;
    if(argc==2 && !std::strcmp(argv[1],"--texture-benchmark")){
        auto &renderer=bozo::render::Renderer::instance();renderer.setDrawDigest(false);
        constexpr unsigned lookups=2000;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<lookups;++i)draw(*rt,registry);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::printf("Texture lookups: %u x 256 KiB, %.3f ms total, %.3f us/lookup\n",lookups,ms,ms*1000/lookups);
        return failures?1:0;
    }
    word(ram,otherPixels+256*256*4-4,0x80008000);draw(*rt,registry);
    const auto afterEdit=bozo::render::Renderer::instance().published()->batches[0].texture;
    check(afterEdit->rgba.back()==0x80008000 && afterEdit->version!=beforeEdit->version,
          "end-of-image texel edit invalidates the decoded image immediately");
    check(beforeEdit->rgba.back()==0x80000080,"an earlier submitted image keeps its original texels");
    // Palettes are checked independently even when indexed texels are unchanged.
    word(ram,duplicate,(16u<<16)|13);word(ram,duplicate+4,(1u<<16)|16u);
    const uint64_t indexed=0x500u|(uint64_t(0x13)<<20)|(uint64_t(4)<<26)|(uint64_t(4)<<30)|(uint64_t(1)<<34);
    std::memcpy(ram+duplicate+0x28,&indexed,8);
    constexpr uint32_t palette=0x4c0000;
    word(ram,duplicate+0x48,palette);word(ram,duplicate+0x4c,0);
    std::memset(ram+otherPixels,1,256);word(ram,palette+4,0x80000080);
    build(*rt,registry);check(firstTexel()==0x80000080,"plain indexed image uses its current palette");
    word(ram,palette+4,0x80008000);
    check(firstTexel()==0x80008000,"palette-only edit remains visible on the next lookup");
    // Guest physical addresses retain their RAM alias/wrap semantics in the hash.
    picture(duplicate,"floor",0x1fffff0,0x80000080);
    word(ram,duplicate,4u<<16);word(ram,duplicate+4,(4u<<16)|2u);
    const uint64_t wrapped=0x500u|(uint64_t(2)<<26)|(uint64_t(1)<<30)|(uint64_t(1)<<34);
    std::memcpy(ram+duplicate+0x28,&wrapped,8);
    for(unsigned i=0;i<8;++i)word(ram,(0x1fffff0+i*4)&0x1ffffff,0x80000080);
    build(*rt,registry);draw(*rt,registry);word(ram,12,0x80008000);draw(*rt,registry);
    check(bozo::render::Renderer::instance().published()->batches[0].texture->rgba.back()==0x80008000,
          "texture validation observes writes past the RAM wrap boundary");
    // A loaded picture owns its image after its source file buffer is released.
    // Its native pin must migrate during the real cleanup boundary too.
    std::memset(ram+floor,0,3*0x50);word(ram,manager,3);
    picture(floor,"menu",menuPixels,0x80008000);picture(moved,"floor",floorPixels,0x80000080);
    R5900Context load;load.pc=0x154950;load.r[4].uw[0]=0x480000;load.r[5].uw[0]=moved;
    load.r[6].uw[0]=0x500;load.r[31].uw[0]=0x123450;
    registry.find(load.pc)(ram,&load,rt.get());check(load.pc==0x123450,"native texture load pins the parsed image");
    for(int i=0;i<4;++i)word(ram,floorPixels+i*4,0x80008000);
    build(*rt,registry);check(firstTexel()==red,"loaded image survives source-buffer reuse");
    std::memset(ram+floor,0,0x50);
    R5900Context cleanup;cleanup.pc=0x133a60;cleanup.r[4].uw[0]=manager;cleanup.r[31].uw[0]=0x123450;
    check(registry.find(cleanup.pc)!=nullptr,"texture compaction lifetime owner installed");
    if(auto fn=registry.find(cleanup.pc))fn(ram,&cleanup,rt.get());
    check(cleanup.pc==0x123450,"original CPU cleanup returns normally");
    picture(moved,"menu",otherPixels,0x80800000);
    check(firstTexel()==red,"pinned native image follows its relocated texture record after source-buffer release");
    std::printf("DC model lifetime: %d failures\n",failures);
    return failures ? 1 : 0;
}
