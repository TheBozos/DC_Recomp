// Synthetic scene-copy and height-field inputs exercise the production native
// water owners. No disc, console packets or microprogram execution.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#ifdef DC_WATER_GPU
#include "bozo/render/gl.h"
#include "bozo/app/png.h"
#include <SDL3/SDL.h>
#endif
namespace dc::native { void install(Runtime &, bozo::port::Registry &); }
namespace {
constexpr uint32_t water=0x400000, heights=0x410000, world=0x420000, info=0x430000,
    copySource=0x440000, copyDest=copySource+16, rect=copySource+32,
    manager=0x1c75870, work=manager+0x10f8;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
int failures=0;
void check(bool ok,const char *what){if(!ok){++failures;std::printf("FAIL %s\n",what);}}
void word(uint8_t *r,uint32_t a,uint32_t v){std::memcpy(r+a,&v,4);}
void real(uint8_t *r,uint32_t a,float v){word(r,a,std::bit_cast<uint32_t>(v));}
void quadword(uint8_t *r,uint32_t a,uint64_t v){std::memcpy(r+a,&v,8);}
void frameTex(uint8_t *r,R5900Context *c,Runtime *)
{quadword(r,c->r[4].uw[0],0x1000);c->pc=c->r[31].uw[0];}
void call(Runtime &rt,bozo::port::Registry &reg,uint32_t pc,R5900Context &c)
{c.pc=pc;c.r[31].uw[0]=0x123450;auto fn=reg.find(pc);check(fn!=nullptr,"native owner installed");if(fn)fn(rt.memory.ram(),&c,&rt);check(c.pc==0x123450 && !c.leaving,"native owner returns");}
void copy(Runtime &rt,bozo::port::Registry &reg)
{
    R5900Context c;c.r[4].uw[0]=copySource;call(rt,reg,0x12f130,c);
    c.r[5].uw[0]=rect;c.r[6].uw[0]=copyDest;
    call(rt,reg,0x12f210,c);
}
void draw(Runtime &rt,bozo::port::Registry &reg)
{
    R5900Context c;c.r[4].uw[0]=water+0x70;c.r[5].uw[0]=0x12000000;
    c.r[6].uw[0]=world;c.r[7].uw[0]=info;c.r[8].uw[0]=15;
    call(rt,reg,0x135000,c);
}
bool near(float a,float b){return std::abs(a-b)<0.00003f;}
}
int main(int argc,char **argv)
{
    auto rt=std::make_unique<Runtime>();auto *r=rt->memory.ram();
    const BozoFunctionEntry originals[]={{0x12f130,frameTex},{0x12f1a0,frameTex}};
    rt->functions.setGenerated(originals,2);bozo::port::Registry reg;dc::native::install(*rt,reg);
    word(r,manager,0);std::strcpy(reinterpret_cast<char *>(r+work+8),"work");
    std::strcpy(reinterpret_cast<char *>(r+0x29a0b8),"work");
    quadword(r,work+0x28,0x1a40);quadword(r,copyDest,0x1a40);
    word(r,rect+8,640);word(r,rect+12,224);
    word(r,water,3);word(r,water+4,3);word(r,water+8,heights);word(r,water+0x90,0x80786e64);
    real(r,water+0xa0,8);
    const float corners[3][4]={{-.8f,0,.2f,1},{-.8f,0,.8f,1},{.8f,0,.2f,1}};
    std::memcpy(r+water+0x20,corners,sizeof(corners));
    std::memcpy(r+world,identity,sizeof(identity));std::memcpy(r+info+0x10,identity,sizeof(identity));
    // Top-down projection, with a varying homogeneous w to expose a second
    // perspective divide of screen-derived texture coordinates.
    real(r,info+0x10+5*4,0);real(r,info+0x10+9*4,1);
    real(r,info+0x10+10*4,0);real(r,info+0x10+11*4,.5f);
    std::memcpy(r+info+0x230,identity,sizeof(identity));std::memcpy(r+info+0x270,identity,sizeof(identity));
    real(r,info+0x270,320);real(r,info+0x270+5*4,112);
    real(r,info+0x270+10*4,0);real(r,info+0x270+12*4,2048);
    real(r,info+0x270+13*4,2048);real(r,info+0x270+14*4,4000000);
    word(r,info+0x310,2);
    R5900Context create;create.r[4].uw[0]=water;create.r[5].uw[0]=0x12000000;
    call(*rt,reg,0x1607e0,create);
    auto &renderer=bozo::render::Renderer::instance();
    renderer.beginFrame({1728,1936,640,224});draw(*rt,reg);renderer.endFrame();
    auto f=renderer.published();check(f && f->batches.size()==1,"unproduced work still draws water");
    if(f && !f->batches.empty())check(!f->batches.back().material.textured && !f->batches.back().material.fromFrame,
        "unproduced work never guesses a current/previous scene source");
    for(bool ripples:{false,true})
    {
        for(int i=0;i<12;++i)real(r,heights+i*4,ripples ? float(i/3)*.2f+float(i%3)*.1f : 0);
        const std::vector<uint8_t> before(r,r+bozo::kFlatScratchpadOffset);
        renderer.beginFrame({1728,1936,640,224});copy(*rt,reg);draw(*rt,reg);renderer.endFrame();
        // The sole guest write is the original frame getter's output TEX0.
        check(std::memcmp(r+water,before.data()+water,0x200)==0,"water draw preserves guest object and packet sentinel");
        f=renderer.published();check(f && f->batches.size()==2,"actual scene producer precedes water");
        if(!f || f->batches.size()!=2)continue;
        const auto &producer=f->batches[0], &batch=f->batches[1];const auto &m=batch.material;
        check(producer.material.fromFrame && producer.material.source==0 && producer.material.target!=0,
            "producer snapshots the current frame");
        check(m.textured && m.fromFrame && m.source==producer.material.target && m.source!=8 && m.target==0,
            "water samples its actual scene-copy target");
        check(m.depthTest && !m.depthWrite && m.blend==bozo::render::Material::Blend::Alpha,
            "water retains foreground occlusion, tint blending and no depth writes");
        check(m.frameWidth==640 && m.frameHeight==224 && m.clampU && m.clampV,"frame sampling uses field dimensions and clamps");
        check(std::memcmp(batch.matrix,identity,sizeof(identity))==0,"screen UVs interpolate without another perspective divide");
        check(batch.count==24,"complete height-field tessellation");
        // Strip expansion order: a,b,d,b,e,d for each cell.
        const int indices[]={0,3,1,3,4,1,1,4,2,4,5,2,3,6,4,6,7,4,4,7,5,7,8,5};
        for(unsigned i=0;i<batch.count && i<24;++i)
        {
            const auto &v=f->vertices[batch.first+i];const int index=indices[i], row=index/3;
            float h=0,prev=0,next=0;std::memcpy(&h,r+heights+index*4,4);
            std::memcpy(&prev,r+heights+(row ? index-3:index)*4,4);std::memcpy(&next,r+heights+(index+1)*4,4);
            check(near(v.u,(v.x-1728+8*(prev-h))/640) && near(v.v,(v.y-1936+8*(h-next))/224),
                "screen lookup follows the original row/column height differences");
            check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.u)&&std::isfinite(v.v),"finite projected vertices");
            check(v.rgba==0x80786e64,"water tint and alpha preserved");
        }
    }
    // Cross the near plane: clipping must happen before projecting the water.
    real(r,info+0x230+2*4,4);real(r,info+0x230+14*4,-1);
    renderer.beginFrame({1728,1936,640,224});copy(*rt,reg);draw(*rt,reg);renderer.endFrame();f=renderer.published();
    check(f && f->batches.size()==2 && f->batches.back().count>0,"partially clipped water remains drawable");
    if(f)for(const auto &v:f->vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.u)&&std::isfinite(v.v),"clipped projection remains finite");
    std::memset(r+info+0x230,0,sizeof(identity));
    renderer.beginFrame({1728,1936,640,224});copy(*rt,reg);draw(*rt,reg);renderer.endFrame();f=renderer.published();
    check(f && f->batches.size()==1,"zero-w water does not submit invalid projected triangles");
#ifdef DC_WATER_GPU
    if(argc>1)
    {
        if(!SDL_Init(SDL_INIT_VIDEO))return 1;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        auto *window=SDL_CreateWindow("Native water refraction",640,224,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(!window)return 1;auto context=SDL_GL_CreateContext(window);
        if(!context || !bozo::render::gl::load([](const char *n){return reinterpret_cast<void *>(SDL_GL_GetProcAddress(n));}))return 1;
        auto &gl=bozo::render::gl::api();
        std::memcpy(r+info+0x230,identity,sizeof(identity));real(r,info+0x10+11*4,0);
        word(r,water+0x90,0x80808080);
        auto background=[&](float x0,float y0,float x1,float y1,float z,uint32_t colour,bool writesDepth=false) {
            bozo::render::Material m;m.textured=false;m.depthTest=false;m.depthWrite=writesDepth;
            m.blend=bozo::render::Material::Blend::Opaque;
            bozo::render::Vertex v[6];const float xy[][2]={{x0,y0},{x1,y0},{x0,y1},{x1,y0},{x1,y1},{x0,y1}};
            for(int i=0;i<6;++i){v[i].x=1728+xy[i][0];v[i].y=1936+xy[i][1];v[i].z=z;v[i].rgba=colour;}
            renderer.draw(m,identity,v,6);
        };
        auto pixel=[&](int x,int y,int red,int green,int blue,const char *what) {
            uint8_t p[4];gl.ReadPixels(x,223-y,1,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,p);
            const bool ok=std::abs(int(p[0])-red)<=3 && std::abs(int(p[1])-green)<=3 && std::abs(int(p[2])-blue)<=3;
            if(!ok)std::printf("pixel %d,%d = %u,%u,%u expected %d,%d,%d\n",x,y,p[0],p[1],p[2],red,green,blue);
            check(ok,what);
        };
        for(bool ripples:{false,true})
        {
            for(int i=0;i<12;++i)real(r,heights+i*4,ripples ? float(i/3)*8 : 0);
            renderer.beginFrame({1728,1936,640,224});
            background(0,0,320,224,0,0x80000080);background(320,0,640,224,0,0x80800000);
            copy(*rt,reg);
            // Foreground is absent from the captured image, but must still occlude water.
            background(270,155,290,175,8000000,0x80008000,true);
            draw(*rt,reg);renderer.endFrame();gl.Viewport(0,0,640,224);
            check(renderer.present(0,0,640,224),"native water presents");
            pixel(280,165,0,255,0,"foreground occludes refracted scene");
            pixel(20,185,255,0,0,"water leaves outside pixels untouched");
            pixel(350,185,ripples?255:0,0,ripples?0:255,"height differences displace the captured scene");
            pixel(450,185,0,0,255,"water retains scene detail rather than flat colour");
            if(argc>2 && ripples)
            {
                std::vector<uint8_t> pixels(640*224*4),flipped(pixels.size());
                gl.ReadPixels(0,0,640,224,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,pixels.data());
                for(int y=0;y<224;++y)std::memcpy(flipped.data()+y*640*4,pixels.data()+(223-y)*640*4,640*4);
                bozo::app::writePng(argv[2],flipped.data(),640,224);
            }
        }
        renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
    }
#endif
    std::printf("Native water refraction: %d failures\n",failures);return failures?1:0;
}
