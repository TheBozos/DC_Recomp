// Synthetic shadow geometry through the native pass. Unmarked target pixels
// must stay transparent, including overlap and same-frame target reuse.
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
namespace dc::native { void install(Runtime &,bozo::port::Registry &); void publish(); }
namespace {
constexpr uint32_t visual=0x400000,asset=0x410000,world=0x420000,info=0x430000;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
int failures=0;
void check(bool ok,const char *s){if(!ok){++failures;std::printf("FAIL %s\n",s);}}
void word(uint8_t *r,uint32_t at,uint32_t n){std::memcpy(r+at,&n,4);}
void real(uint8_t *r,uint32_t at,float n){word(r,at,std::bit_cast<uint32_t>(n));}
void call(Runtime &rt,bozo::port::Registry &reg,uint32_t pc,R5900Context &c)
{c.pc=pc;c.r[31].uw[0]=0x123450;reg.find(pc)(rt.memory.ram(),&c,&rt);check(c.pc==0x123450 && !c.leaving,"shadow owner returns");}
void quad(float x0,float y0,float x1,float y1,float z,uint32_t colour,bool depth)
{
    bozo::render::Material m;m.textured=false;m.blend=bozo::render::Material::Blend::Opaque;
    m.depthTest=false;m.depthWrite=depth;bozo::render::Vertex v[6];
    const float xy[6][2]={{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
    for(int i=0;i<6;++i){v[i].x=1728+xy[i][0];v[i].y=1936+xy[i][1];v[i].z=z;v[i].rgba=colour;}
    bozo::render::Renderer::instance().draw(m,identity,v,6);
}
void pixel(int x,int y,int red,int green,int blue,const char *what)
{
    uint8_t p[4];bozo::render::gl::api().ReadPixels(x,223-y,1,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,p);
    const auto near=[](int a,int b){return a>=b-3 && a<=b+3;};
    if(!(near(p[0],red)&&near(p[1],green)&&near(p[2],blue)))
        std::printf("pixel (%d,%d): %u %u %u expected %d %d %d\n",x,y,p[0],p[1],p[2],red,green,blue);
    check(near(p[0],red)&&near(p[1],green)&&near(p[2],blue),what);
}
}
int main(int argc,char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO))return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Native shadow coverage",640,224,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window)return 1;auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *n){return reinterpret_cast<void *>(SDL_GL_GetProcAddress(n));}))return 1;
    auto rt=std::make_unique<Runtime>();auto *ram=rt->memory.ram();bozo::port::Registry reg;dc::native::install(*rt,reg);
    word(ram,asset,0x54444d);word(ram,asset+0x10,0x40);word(ram,asset+0x28,0x80);
    const float positions[3][4]={{-.5f,1,-.5f,1},{.5f,2,-.5f,1},{0,1,.5f,1}};
    std::memcpy(ram+asset+0x40,positions,sizeof(positions));word(ram,asset+0x88,1);
    word(ram,asset+0x90,3);word(ram,asset+0x94,3);
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)word(ram,asset+0x9c+i*12+j*4,i);
    word(ram,visual+0x20,asset);std::memcpy(ram+world,identity,sizeof(identity));
    std::memcpy(ram+info+0x10,identity,sizeof(identity));real(ram,info+0x10+5*4,0);real(ram,info+0x10+10*4,0);
    real(ram,info+0x10+6*4,1);real(ram,info+0x10+9*4,1); // top-down X/Z camera
    std::memcpy(ram+info+0x230,identity,sizeof(identity));std::memcpy(ram+info+0x270,identity,sizeof(identity));
    real(ram,info+0x270,320);real(ram,info+0x270+5*4,112);real(ram,info+0x270+10*4,0);
    real(ram,info+0x270+12*4,2048);real(ram,info+0x270+13*4,2048);real(ram,info+0x270+14*4,8000000);
    std::memcpy(ram+info+0x2d0,identity,sizeof(identity));real(ram,info+0x2d0+5*4,0);word(ram,info+0x320,1);
    auto &renderer=bozo::render::Renderer::instance();
    for(int scenario=0;scenario<4;++scenario)
    {
        renderer.beginFrame({1728,1936,640,224});quad(0,0,640,224,0,0x80800000,false);
        if(scenario==2)quad(305,100,335,120,12000000,0x80000080,true);
        auto pass=[&](float x,int copies){real(ram,world+12*4,x);R5900Context c;call(*rt,reg,0x1308a0,c);
            for(int i=0;i<copies;++i){c={};c.r[4].uw[0]=visual;c.r[6].uw[0]=world;c.r[7].uw[0]=info;call(*rt,reg,0x136290,c);}
            c={};c.r[4].uw[0]=52;call(*rt,reg,0x130b30,c);};
        if(scenario==3){pass(-.5f,1);pass(.5f,1);}else pass(0,scenario==1?2:1);
        dc::native::publish();check(renderer.present(0,0,640,224),"shadow presents");
        pixel(20,20,0,0,255,"outside shadow leaves scene unchanged");
        if(scenario==2)pixel(320,112,255,0,0,"foreground occludes the shadow");
        else if(scenario==3){pixel(160,105,0,0,151,"first pass does not darken again on target reuse");pixel(480,105,0,0,151,"second pass marks only its own triangle");}
        else pixel(320,105,0,0,151,"shadow darkens once at strength 52, including overlapping draws");
        if(argc==2 && scenario==1)
        {
            std::vector<uint8_t> image(640*224*4),rows(image.size());auto &gl=bozo::render::gl::api();
            gl.ReadPixels(0,0,640,224,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,image.data());
            for(int y=0;y<224;++y)std::memcpy(rows.data()+y*640*4,image.data()+(223-y)*640*4,640*4);
            bozo::app::writePng(argv[1],rows.data(),640,224);
        }
    }
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("Native shadow mask: %d failures\n",failures);return failures?1:0;
}
