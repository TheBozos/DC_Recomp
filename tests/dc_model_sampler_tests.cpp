// Native MDT models inherit SetEnv's edge clamps. A mirrored join at UV zero
// must not blend in the opposite atlas edge. Synthetic assets only.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include "bozo/render/gl.h"
#include "bozo/app/png.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace {
constexpr uint32_t visual=0x400000,asset=0x410000,matrix=0x420000,info=0x430000,pixels=0x440000;
constexpr uint32_t manager=0x1c75870,record=manager+0x10f8;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void word(uint8_t *r,uint32_t p,uint32_t v){std::memcpy(r+p,&v,4);}
void scalar(uint8_t *r,uint32_t p,float v){std::memcpy(r+p,&v,4);}
void call(Runtime &rt,bozo::port::Registry &registry,uint32_t pc,R5900Context &c)
{c.pc=pc;c.r[31].uw[0]=0x123450;registry.find(pc)(rt.memory.ram(),&c,&rt);}
}
int main(int argc,char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO))return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Native model mirrored atlas join",512,512,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *s)->void*{return reinterpret_cast<void*>(SDL_GL_GetProcAddress(s));}))return 1;
    auto rt=std::make_unique<Runtime>();bozo::port::Registry registry;dc::native::install(*rt,registry);
    auto *r=rt->memory.ram();auto &renderer=bozo::render::Renderer::instance();auto &gl=bozo::render::gl::api();
    renderer.setEnabled(true);renderer.setPresentsAlone(true);
    std::memcpy(r+matrix,identity,sizeof(identity));std::memcpy(r+info+0x10,identity,sizeof(identity));
    std::memcpy(r+info+0x230,identity,sizeof(identity));std::memcpy(r+info+0x270,identity,sizeof(identity));
    scalar(r,info+0x270,8);scalar(r,info+0x270+20,8); // clip [-1,1] to logical [0,16]
    scalar(r,info+0x270+48,8);scalar(r,info+0x270+52,8);
    word(r,info+0x310,2);scalar(r,info+0x1a0,1);
    for(unsigned i=0;i<4;++i)scalar(r,info+0x190+i*4,128);
    word(r,asset,0x0054444d);word(r,asset+8,0x240);word(r,asset+0x10,0x40);
    word(r,asset+0x18,0xa0);word(r,asset+0x28,0x110);word(r,asset+0x30,0xb0);word(r,asset+0x38,0x1c0);
    const float xy[6][2]={{-1,-1},{0,-1},{1,-1},{-1,1},{0,1},{1,1}};
    for(unsigned i=0;i<6;++i){scalar(r,asset+0x40+i*16,xy[i][0]);scalar(r,asset+0x44+i*16,xy[i][1]);}
    scalar(r,asset+0xa8,1);word(r,asset+0x118,1);word(r,asset+0x120,3);word(r,asset+0x124,12);
    const unsigned indices[12]={0,1,3,1,4,3,1,2,4,2,5,4};
    for(unsigned i=0;i<12;++i){word(r,asset+0x12c+i*12,indices[i]);word(r,asset+0x134+i*12,indices[i]);}
    for(unsigned i=0;i<4;++i)scalar(r,asset+0x1c0+i*4,1);
    std::strcpy(reinterpret_cast<char *>(r+asset+0x1f4),"face");
    word(r,record,8u<<16);word(r,record+4,(4u<<16)|8u);
    std::strcpy(reinterpret_cast<char *>(r+record+8),"face");
    const uint64_t tex0=0x2000u|(uint64_t(3)<<26)|(uint64_t(3)<<30);
    std::memcpy(r+record+0x28,&tex0,8);word(r,record+0x38,pixels);word(r,record+0x3c,256);
    word(r,visual+8,0x2a11a0);word(r,visual+0x20,asset);
    R5900Context build;build.r[4].uw[0]=visual;build.r[5].uw[0]=0x12000000;build.r[6].uw[0]=asset;build.r[28].uw[0]=0x500000;
    call(*rt,registry,0x135aa0,build);
    int failures=0;
    for(unsigned mode=0;mode<7;++mode)for(int side:{64,256,512}){
        const bool vAxis=mode==1 || mode==3;
        word(r,info+0x340,mode>=5?1:0);
        // The sky-gradient path disables texture mapping. Its RGB is already
        // a display colour, while its alpha still uses 0x80 as full strength.
        scalar(r,asset+0x1c0,mode>=5?.25f:1.f);
        scalar(r,asset+0x1c4,mode>=5?.5f:1.f);
        scalar(r,asset+0x1c8,mode>=5?.75f:1.f);
        scalar(r,asset+0x1cc,mode==6?.5f:1.f);
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
            word(r,pixels+(y*8+x)*4,(vAxis?y:x)==7?0x80000000:0x80808080);
        for(unsigned i=0;i<6;++i){
            float u=.25f,v=.25f;
            if(mode==0)u=std::abs(xy[i][0])*.25f;
            if(mode==1)v=std::abs(xy[i][0])*.25f;
            if(mode==2)u=1.125f;
            if(mode==3)v=1.125f;
            scalar(r,asset+0xb0+i*16,u);scalar(r,asset+0xb4+i*16,v);
        }
        renderer.beginFrame({0,0,16,16});
        R5900Context draw;draw.r[4].uw[0]=visual;draw.r[6].uw[0]=matrix;draw.r[7].uw[0]=info;draw.r[8].uw[0]=6;
        call(*rt,registry,0x135000,draw);renderer.endFrame();gl.Viewport(0,0,side,side);
        if(!renderer.present(0,0,float(side),float(side)))return 1;
        uint8_t samples[8]{};gl.ReadPixels(side/2-1,side/2,2,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,samples);
        const int ordinary=mode==2 || mode==3?0:128;
        const int expected[3]={mode>=5?(mode==6?16:32):ordinary,mode>=5?(mode==6?32:64):ordinary,mode>=5?(mode==6?48:96):ordinary};bool bad=false;
        for(unsigned i=0;i<2;++i)for(unsigned c=0;c<3;++c)bad|=std::abs(int(samples[i*4+c])-expected[c])>3;
        if(bad){++failures;std::printf("FAIL model sampler mode %u size %d: centre %u,%u,%u / %u,%u,%u expected %d,%d,%d\n",mode,side,samples[0],samples[1],samples[2],samples[4],samples[5],samples[6],expected[0],expected[1],expected[2]);}
        if(argc==2 && mode==0 && side==512){
            std::vector<uint8_t> image(side*side*4),flipped(image.size());gl.ReadPixels(0,0,side,side,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,image.data());
            for(int y=0;y<side;++y)std::memcpy(flipped.data()+y*side*4,image.data()+(side-1-y)*side*4,side*4);
            bozo::app::writePng(argv[1],flipped.data(),side,side);
        }
    }
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("Native model sampler: %d failures\n",failures);return failures?1:0;
}
