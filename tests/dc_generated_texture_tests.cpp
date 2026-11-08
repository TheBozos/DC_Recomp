// Actual native model/composite/sprite hooks with invented records and GPU
// readback. A colour-only fill must not make a generated flame opaque.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include "bozo/render/gl.h"
#include "bozo/app/png.h"
#include <SDL3/SDL.h>
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
namespace dc::native { void install(Runtime &,bozo::port::Registry &); }
namespace {
constexpr uint32_t manager=0x1c75870,records=manager+0x10f8,pixels=0x400000,rect=0x410000,
    point=0x420000,stack=0x430000,asset=0x440000,visual=0x450000,matrix=0x460000,info=0x470000;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
int failures=0;
void check(bool ok,const char *s) {if(!ok){++failures;std::printf("FAIL %s\n",s);}}
void word(uint8_t *ram,uint32_t at,uint32_t n){std::memcpy(ram+at,&n,4);}
void real(uint8_t *ram,uint32_t at,float n){word(ram,at,std::bit_cast<uint32_t>(n));}
void wide(uint8_t *ram,uint32_t at,uint64_t n){std::memcpy(ram+at,&n,8);}
void rectangle(uint8_t *ram,uint32_t at,int x,int y,int w,int h)
{word(ram,at,x);word(ram,at+4,y);word(ram,at+8,w);word(ram,at+12,h);}
void call(Runtime &rt,bozo::port::Registry &registry,uint32_t pc,R5900Context &c)
{c.pc=pc;c.r[31].uw[0]=0x123450;registry.find(pc)(rt.memory.ram(),&c,&rt);check(c.pc==0x123450 && !c.leaving,"native hook returns");}
void background()
{
    bozo::render::Material m;m.depthTest=m.depthWrite=false;m.blend=bozo::render::Material::Blend::Opaque;
    bozo::render::Vertex v[6];const float xy[6][2]={{0,0},{640,0},{0,224},{640,0},{640,224},{0,224}};
    for(int i=0;i<6;++i){v[i].x=1728+xy[i][0];v[i].y=1936+xy[i][1];v[i].rgba=0x80800000;}
    bozo::render::Renderer::instance().draw(m,identity,v,6);
}
}
int main(int argc,char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO))return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Generated flame regression",640,224,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window)return 1;auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *s){return reinterpret_cast<void *>(SDL_GL_GetProcAddress(s));}))return 1;
    auto rt=std::make_unique<Runtime>();auto *ram=rt->memory.ram();bozo::port::Registry registry;
    dc::native::install(*rt,registry);auto &renderer=bozo::render::Renderer::instance();
    word(ram,manager,2);
    for(unsigned i=0;i<3;++i)
    {
        const auto record=records+i*0x50;
        word(ram,record,16u<<16);word(ram,record+4,(4u<<16)|16u);
        std::strcpy(reinterpret_cast<char *>(ram+record+8),i==0?"a":i==1?"b":"work");
        wide(ram,record+0x28,(i==2?0x1a40u:0x2000u+i*0x100u)|(uint64_t(4)<<26)|(uint64_t(4)<<30)|(uint64_t(1)<<34));
        if(i<2){word(ram,record+0x38,pixels+i*0x1000);word(ram,record+0x3c,16*16*4);}
    }
    for(unsigned i=0;i<16*16;++i)
    {
        const unsigned x=i%16,y=i/16;
        word(ram,pixels+i*4,(x>=4 && x<12 && y>=4 && y<12)?0x80404040u:0x00404040u);
        word(ram,pixels+0x1000+i*4,(x>=6 && x<10 && y>=6 && y<10)?0x80000040u:0x00ffffffu);
    }
    // An unbacked work record on a live MDT must not sample an unrelated
    // previous scene. No semantic producer has registered this work image.
    word(ram,asset,0x0054444d);word(ram,asset+8,0x240);word(ram,asset+0x10,0x40);
    word(ram,asset+0x18,0x70);word(ram,asset+0x28,0xd0);word(ram,asset+0x30,0xa0);word(ram,asset+0x38,0x180);
    const float positions[3][4]={{-.5f,-.5f,0,1},{.5f,-.5f,0,1},{0,.5f,0,1}};
    std::memcpy(ram+asset+0x40,positions,sizeof(positions));
    for(unsigned i=0;i<3;++i)real(ram,asset+0x70+i*16+8,1);
    for(unsigned i=0;i<4;++i)real(ram,asset+0x180+i*4,1);
    std::strcpy(reinterpret_cast<char *>(ram+asset+0x180+0x34),"work");
    word(ram,asset+0xd8,1);word(ram,asset+0xe0,3);word(ram,asset+0xe4,3);word(ram,asset+0xe8,0);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)word(ram,asset+0xec+i*12+j*4,i);
    word(ram,visual+8,0x2a11a0);word(ram,visual+0x20,asset);
    R5900Context c;c.r[4].uw[0]=visual;c.r[5].uw[0]=0x12000000;c.r[6].uw[0]=asset;c.r[28].uw[0]=0x300000;
    call(*rt,registry,0x135aa0,c);
    for(auto at:{matrix,info+0x10,info+0x230,info+0x270})std::memcpy(ram+at,identity,sizeof(identity));
    word(ram,info+0x310,2);real(ram,info+0x1a0,1);
    for(unsigned i=0;i<4;++i)real(ram,info+0x190+i*4,128);
    renderer.beginFrame({});c={};c.r[4].uw[0]=visual;c.r[5].uw[0]=0x12000000;c.r[6].uw[0]=matrix;c.r[7].uw[0]=info;c.r[8].uw[0]=6;
    call(*rt,registry,0x135000,c);renderer.endFrame();
    auto model=renderer.published();check(model && model->batches.size()==1,"unbacked model still submits its geometry");
    check(model && !model->batches.empty() && model->batches[0].material.fromFrame==0,"unproduced work record never guesses previous-frame content");
    rectangle(ram,rect,0,0,16,16);wide(ram,rect+0x20,0x44);
    for(bool inset:{false,true})for(bool fog:{false,true})for(unsigned alpha:{0u,64u,128u})
    {
        renderer.beginFrame({1728,1936,640,224});background();
        c={};c.r[4].uw[0]=rect+0x20;call(*rt,registry,0x12ef90,c);
        c={};c.r[5].uw[0]=0x1a40;c.r[6].uw[0]=640;c.r[8].uw[0]=records;c.r[9].uw[0]=rect;
        c.r[10].uw[0]=rect;c.r[11].uw[0]=records+0x50;c.r[29].uw[0]=stack;
        rectangle(ram,rect+0x40,inset?4:0,inset?4:0,inset?8:16,inset?8:16);
        word(ram,stack,rect+0x40);word(ram,stack+8,rect);call(*rt,registry,0x162580,c);
        wide(ram,rect+0x20,0x48);c={};c.r[4].uw[0]=rect+0x20;call(*rt,registry,0x12ef90,c);
        wide(ram,rect+0x20,0x44);
        const int xy[4][2]={{20,20},{180,20},{20,180},{180,180}};
        for(unsigned i=0;i<4;++i){word(ram,point+i*16,(1728+xy[i][0])*16);word(ram,point+i*16+4,(1936+xy[i][1])*16);word(ram,point+i*16+8,100);word(ram,point+i*16+12,255);}
        c={};c.r[4].uw[0]=0x12000000;c.r[5].uw[0]=records+0xa0;c.r[6].uw[0]=rect;
        for(unsigned i=0;i<4;++i)c.r[7+i].uw[0]=point+i*16;
        word(ram,rect+0x30,0x00808080|(alpha<<24));c.r[11].uw[0]=fog?alpha:rect+0x30;
        call(*rt,registry,fog?0x15d9a0:0x15d4b0,c);renderer.endFrame();
        check(renderer.present(0,0,640,224),"generated flame presents");
        uint8_t corner[4]{},centre[4]{};auto &gl=bozo::render::gl::api();
        gl.ReadPixels(40,223-40,1,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,corner);
        gl.ReadPixels(100,223-100,1,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,centre);
        check(corner[0]<4 && corner[1]<4 && corner[2]>250,"transparent generated texel leaves blue background untouched");
        check(alpha?centre[0]>20:centre[0]<4,"flame centre follows sprite alpha");
    }
    if(argc==2)
    {
        std::vector<uint8_t> rgba(640*224*4),rows(rgba.size());
        bozo::render::gl::api().ReadPixels(0,0,640,224,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,rgba.data());
        for(int y=0;y<224;++y)std::memcpy(rows.data()+y*640*4,rgba.data()+(223-y)*640*4,640*4);
        bozo::app::writePng(argv[1],rows.data(),640,224);
    }
    std::printf("Generated texture provenance/alpha: %d failures\n",failures);
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();return failures?1:0;
}
