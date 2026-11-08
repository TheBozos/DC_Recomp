// Native rectangle owners must tile without exposing the background at scaled
// pixel centres. Invented atlas: white tile next to a contrasting magenta tile.
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
constexpr uint32_t manager=0x1c75870, record=manager+0x10f8, pixels=0x400000,
    dest=0x410000, source=dest+0x20, colour=dest+0x40, table=dest+0x100, entries=dest+0x200;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void word(uint8_t *ram,uint32_t at,uint32_t v){std::memcpy(ram+at,&v,4);}
void half(uint8_t *ram,uint32_t at,uint16_t v){std::memcpy(ram+at,&v,2);}
void rect(uint8_t *ram,uint32_t at,int x,int y,int w,int h)
{word(ram,at,x);word(ram,at+4,y);word(ram,at+8,w);word(ram,at+12,h);}
void call(Runtime &rt,bozo::port::Registry &r,uint32_t pc,R5900Context &c)
{c.pc=pc;c.r[31].uw[0]=0x123450;r.find(pc)(rt.memory.ram(),&c,&rt);}
}
int main(int argc,char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO))return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Native sprite seams",1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window)return 1;auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *n){return reinterpret_cast<void *>(SDL_GL_GetProcAddress(n));}))return 1;
    auto rt=std::make_unique<Runtime>();auto *ram=rt->memory.ram();bozo::port::Registry registry;dc::native::install(*rt,registry);
    word(ram,manager,0);word(ram,record,32u<<16);word(ram,record+4,(4u<<16)|16u);
    std::strcpy(reinterpret_cast<char *>(ram+record+8),"atlas");
    const uint64_t tex=0x2000u|(uint64_t(5)<<26)|(uint64_t(4)<<30);
    std::memcpy(ram+record+0x28,&tex,8);word(ram,record+0x38,pixels);word(ram,record+0x3c,32*16*4);
    for(unsigned i=0;i<32*16;++i)word(ram,pixels+i*4,i%32<16?0x80808080:0x80800080);
    rect(ram,source,0,0,16,16);word(ram,colour,0x40808080);
    auto &renderer=bozo::render::Renderer::instance();auto &gl=bozo::render::gl::api();int failures=0;
    for(int height:{448,720,1017,1080})for(uint32_t hook:{0x15c310u,0x15c630u,0x15c980u,0x15e120u,0x12ba20u})
    {
        R5900Context env;env.r[28].uw[0]=0x500000;word(ram,0x500000-0x7408,0x44);
        call(*rt,registry,0x140da0,env); // game's nearest-filter flag is zero
        renderer.beginFrame({1728,1936,640,224});
        bozo::render::Material bg;bg.depthTest=bg.depthWrite=false;bg.blend=bozo::render::Material::Blend::Opaque;
        bozo::render::Vertex v[6];const float xy[6][2]={{0,0},{640,0},{0,224},{640,0},{640,224},{0,224}};
        for(int i=0;i<6;++i){v[i].x=1728+xy[i][0];v[i].y=1936+xy[i][1];v[i].rgba=0x80800000;}
        renderer.draw(bg,identity,v,6);
        // Continuous rows across both map and dialog positions, using translucent
        // tiles so accidental overlap is visible as well as uncovered columns.
        for(int i=0;i<36;++i)
        {
            const int x=44+i*16;rect(ram,dest,x,100,16,16);R5900Context c;
            c.r[5].uw[0]=record;c.r[6].uw[0]=dest;c.r[7].uw[0]=source;c.r[8].uw[0]=64;
            if(hook==0x15c630){c.r[8].uw[0]=c.r[9].uw[0]=c.r[10].uw[0]=128;c.r[11].uw[0]=64;}
            if(hook==0x15c980){for(int j=8;j<=11;++j)c.r[j].uw[0]=colour;c.r[29].uw[0]=dest+0x80;word(ram,dest+0x80,1);}
            if(hook==0x15e120){c.r[5].uw[0]=dest;for(int j=6;j<=9;++j)c.r[j].uw[0]=colour;}
            if(hook==0x12ba20)
            {
                const uint32_t e=entries+i*0x40;half(ram,e,x);half(ram,e+2,100);half(ram,e+4,16);half(ram,e+6,16);
                half(ram,e+8,0);half(ram,e+10,0);half(ram,e+12,16);half(ram,e+14,16);
                for(int j=0;j<4;++j)half(ram,e+16+j*2,j==3?64:128);
                std::memcpy(ram+e+0x28,&tex,8);word(ram,e+0x30,i==35?0:e+0x40);
            }
            else call(*rt,registry,hook,c);
        }
        if(hook==0x12ba20){word(ram,table+4,1);word(ram,table+0xc,entries);word(ram,table+0x4c,0);R5900Context c;c.r[4].uw[0]=table;call(*rt,registry,hook,c);}
        renderer.endFrame();const float width=height*4.0f/3.0f, left=(1920-width)*.5f;
        gl.Viewport(0,0,1920,height);
        if(!renderer.present(left,0,width,height))return 1;
        const int y=static_cast<int>(height*54.0f/224.0f);
        std::vector<uint8_t> row(1920*4);
        // ReadPixels is bottom-up; sprites use half-height logical coordinates.
        gl.ReadPixels(0,height-1-y,1920,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,row.data());
        int bad=0;for(int x=static_cast<int>(std::ceil(left+46*width/640));x<left+618*width/640;++x)
            if(row[x*4]<60 || row[x*4]>68 || row[x*4+1]<60 || row[x*4+1]>68 || row[x*4+2]<188 || row[x*4+2]>196)++bad;
        const int outsideLeft=static_cast<int>(std::ceil(left+44*width/640-.5f))-1;
        const int outsideRight=static_cast<int>(std::ceil(left+620*width/640-.5f));
        if(row[outsideLeft*4]>4 || row[outsideRight*4]>4)++bad;
        if(bad){++failures;std::printf("FAIL height %d hook %x: %d uncovered/overlapped/atlas/outside pixels\n",height,hook,bad);}
        if(argc==2 && height==1017 && hook==0x15c630)
        {
            std::vector<uint8_t> image(1920*height*4),flipped(image.size());
            gl.ReadPixels(0,0,1920,height,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,image.data());
            for(int y0=0;y0<height;++y0)std::memcpy(flipped.data()+y0*1920*4,image.data()+(height-1-y0)*1920*4,1920*4);
            bozo::app::writePng(argv[1],flipped.data(),1920,height);
        }
    }
    // The door-menu atlas has narrow opaque separators inside a wide strip.
    // Pulling only its UV endpoints inward moves those interior separators:
    // at 1080 high, logical x=374 is exactly a covered pixel centre.
    word(ram,record,256u<<16);word(ram,record+4,(4u<<16)|16u);
    const uint64_t wideTex=0x2000u|(uint64_t(8)<<26)|(uint64_t(4)<<30);
    std::memcpy(ram+record+0x28,&wideTex,8);
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<256;++x)
        word(ram,pixels+(y*256+x)*4,(x==194 || x==195)?0x80808080:0x00800080);
    rect(ram,source,28,0,210,16);rect(ram,dest,208,100,210,16);
    for(int height:{448,720,1017,1080}){
        renderer.beginFrame({1728,1936,640,224});
        R5900Context c;c.r[5].uw[0]=record;c.r[6].uw[0]=dest;c.r[7].uw[0]=source;c.r[8].uw[0]=128;
        call(*rt,registry,0x15c310,c);renderer.endFrame();
        // Keep this analytic texel-boundary check on an integer viewport:
        // a fractional letterbox adds a separate final resampling step. The
        // adjacent-tile cases above continue to cover fractional letterboxing.
        const float width=float(std::lround(height*4.0/3.0)),left=std::floor((1920-width)*.5f);
        gl.Viewport(0,0,1920,height);if(!renderer.present(left,0,width,height))return 1;
        std::vector<uint8_t> row(1920*4);const int y=int(height*54.0f/224.0f);
        gl.ReadPixels(0,height-1-y,1920,1,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,row.data());
        int bad=0;
        for(int x=int(std::floor(left+373*width/640));x<left+377*width/640;++x){
            const float logical=(x+.5f-left)*640/width;
            const bool opaque=logical>=374 && logical<376;
            const int expected=opaque?128:0;
            if(std::abs(int(row[x*4])-expected)>3 || std::abs(int(row[x*4+1])-expected)>3){
                if(bad<4)std::printf("  separator pixel %d logical %.8f RGB %u,%u,%u expected %d\n",x,logical,row[x*4],row[x*4+1],row[x*4+2],expected);
                ++bad;
            }
        }
        if(bad){++failures;std::printf("FAIL wide door-menu strip height %d: %d shifted separator samples\n",height,bad);}
    }
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
    std::printf("Native sprite seams: %d failures\n",failures);return failures?1:0;
}
