// Game-free GPU regression through the actual native balloon owner. Synthetic
// records/diamond/tail exercise transparent corners and target reuse.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/gl.h"
#include "bozo/render/renderer.h"
#include "bozo/app/png.h"
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace
{
    constexpr uint32_t cls=0x400000, pixels=0x420000, manager=0x1c75870, record=manager+0x10f8;
    constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    int failures=0;
    void check(bool ok,const char *what) { if(!ok) { ++failures;std::printf("FAIL %s\n",what); } }
    void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
    void number(uint8_t *ram,uint32_t at,float v) { word(ram,at,std::bit_cast<uint32_t>(v)); }
    void quad(bozo::render::Material material,uint32_t colour)
    {
        bozo::render::Vertex v[6];
        const float xy[6][2]={{0,0},{640,0},{0,224},{640,0},{640,224},{0,224}};
        for(int i=0;i<6;++i) { v[i].x=1728+xy[i][0];v[i].y=1936+xy[i][1];v[i].u=xy[i][0]/640;v[i].v=xy[i][1]/224;v[i].rgba=colour; }
        bozo::render::Renderer::instance().draw(material,identity,v,6);
    }
    void pixel(int x,int y,bool paper)
    {
        uint8_t rgba[4]{};bozo::render::gl::api().ReadPixels(x,223-y,1,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,rgba);
        check(paper ? rgba[0]>80 && rgba[1]>80 : rgba[0]<4 && rgba[1]<4 && rgba[2]>250,
              paper ? "parchment covers body/tail" : "outside shape stays transparent over blue");
    }
}
int main(int argc,char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Native balloon mask",640,224,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window) return 1;
    const auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *name){ return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name)); })) return 1;
    auto rt=std::make_unique<Runtime>();auto *ram=rt->memory.ram();bozo::port::Registry registry;
    dc::native::install(*rt,registry);
    word(ram,manager,1);
    std::strcpy(reinterpret_cast<char *>(ram+0x299a78),"balloon");std::strcpy(reinterpret_cast<char *>(ram+record+8),"balloon");
    std::strcpy(reinterpret_cast<char *>(ram+0x299a88),"paper");std::strcpy(reinterpret_cast<char *>(ram+record+0x50+8),"paper");
    const uint64_t balloonTex=0x3000, paperTex=0x2000|(uint64_t(2)<<14)|(uint64_t(7)<<26)|(uint64_t(7)<<30);
    std::memcpy(ram+record+0x28,&balloonTex,8);std::memcpy(ram+record+0x50+0x28,&paperTex,8);
    word(ram,record+0x50,128u<<16);word(ram,record+0x50+4,(4u<<16)|128u);
    word(ram,record+0x50+0x38,pixels);word(ram,record+0x50+0x3c,128*128*4);
    for(unsigned i=0;i<128*128;++i) word(ram,pixels+i*4,0x80a0b0c0);
    const float points[][2]={{.5f,.5f},{0,.5f},{.5f,0},{1,.5f},{.5f,1},{0,.5f}};
    for(unsigned i=0;i<15;++i) for(unsigned c=0;c<2;++c) number(ram,0x253fa0+i*8+c*4,points[i<6?i:5][c]);
    number(ram,cls+0x90,1);word(ram,cls+0x4c,200);word(ram,cls+0x50,100);word(ram,cls+0x16d4,128);
    auto &renderer=bozo::render::Renderer::instance();bozo::render::Screen screen{1728,1936,640,224};
    for(bool tail : {false,true}) for(unsigned x : {40u,300u})
    {
        word(ram,cls+0x3c,x);word(ram,cls+0x44,x);word(ram,cls+0x58,tail);
        word(ram,cls+0x74,x+80);word(ram,cls+0x78,80);word(ram,cls+0x7c,x+120);word(ram,cls+0x80,80);
        word(ram,cls+0x84,x+100);word(ram,cls+0x88,150);
        const std::vector<uint8_t> before(ram,ram+bozo::kFlatScratchpadOffset);
        renderer.beginFrame(screen);
        R5900Context ctx;ctx.r[4].uw[0]=cls;ctx.r[5].uw[0]=0x12000000;ctx.r[31].uw[0]=0x123450;
        const auto initial=ctx;registry.find(0x151ad0)(ram,&ctx,rt.get());
        check(ctx.pc==0x123450 && !ctx.leaving,"native owner returns without a packet");
        auto expected=initial;expected.pc=0x123450;check(!std::memcmp(&ctx,&expected,sizeof(ctx)),"other CPU state preserved");
        check(!std::memcmp(ram,before.data(),before.size()),"balloon consumes records without guest writes");
        bozo::render::Material blue;blue.textured=false;blue.depthTest=blue.depthWrite=false;blue.blend=bozo::render::Material::Blend::Opaque;
        quad(blue,0x80800000);
        auto sample=blue;sample.fromFrame=1;sample.source=1;sample.frameWidth=640;sample.frameHeight=224;sample.blend=bozo::render::Material::Blend::Alpha;
        quad(sample,0x80808080);renderer.endFrame();
        const auto frame=renderer.published();check(frame && frame->batches.size()>=5,"mask, shape, parchment and sample published");
        check(frame && frame->vertices[0].z==screen.depthMax,"mask clear uses the finite native near depth");
        check(renderer.present(0,0,640,224),"native balloon presents");
        pixel(x+100,50,true);pixel(x+5,5,false);pixel(x+100,130,tail);
        if(x==300) pixel(140,50,false); // previous mask must not survive target reuse
    }
    if(argc==2)
    {
        std::vector<uint8_t> rgba(640*224*4),rows(rgba.size());
        bozo::render::gl::api().ReadPixels(0,0,640,224,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,rgba.data());
        for(unsigned y=0;y<224;++y) std::memcpy(rows.data()+y*640*4,rgba.data()+(223-y)*640*4,640*4);
        check(bozo::app::writePng(argv[1],rows.data(),640,224),"write optional diagnostic capture");
    }
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("native balloon mask: %s\n",failures?"FAILED":"passed");return failures?1:0;
}
