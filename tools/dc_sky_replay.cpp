// Isolated original sky draw from a user-supplied RAM capture. Freeze the sky
// clock and rotate only the view. No boot, save writes, packets, or live process.
#include "dc_functions.h"
#include "bozo/host/free_host.h"
#include "bozo/render/renderer.h"
#include "bozo/render/gl.h"
#include "bozo/app/png.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
namespace dc {void install(Runtime &,bozo::port::Registry &);}
namespace {
constexpr uint32_t gp=0x2a97f0,info=0x1c75520,done=0x80000;
void finish(uint8_t *,R5900Context *,Runtime *){throw bozo::ee::EndRun{};}
uint32_t word(const uint8_t *r,uint32_t at){uint32_t v;std::memcpy(&v,r+at,4);return v;}
void doorBoard(uint8_t *r,R5900Context *c,Runtime *rt){
    c->r[31].uw[0]=done;bozo_call_indirect(r,c,rt,0x140da0);
    const uint32_t texture=word(r,gp-0x68c0);
    auto invoke=[&](uint32_t pc){c->pc=pc;c->r[31].uw[0]=done;rt->functions.generated(pc)(r,c,rt);};
    for(int row=0;row<2;++row){
        c->r[4].uw[0]=208;c->r[5].uw[0]=246+row*42;c->r[6].uw[0]=246;
        c->r[7].uw[0]=330;c->r[8].uw[0]=128;c->r[9].uw[0]=texture;invoke(0x225420);
    }
    c->r[4].uw[0]=texture;c->r[5].uw[0]=180;c->r[6].uw[0]=190;c->r[7].uw[0]=128;c->r[8].uw[0]=1;
    invoke(0x224a60);throw bozo::ee::EndRun{};
}
}
int main(int argc,char **argv){
    if(argc!=4){std::fprintf(stderr,"usage: dc-sky-replay RAM OUTPUT_DIR TIME\n");return 2;}
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> snapshot{std::istreambuf_iterator<char>(input),{}};
    if(snapshot.size()!=bozo::kFlatBytes)return 2;
    const bool door=std::string(argv[3])=="door";
    const float clock=door?0:std::stof(argv[3]);std::filesystem::create_directories(argv[2]);
    if(!SDL_Init(SDL_INIT_VIDEO))return 2;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto *window=SDL_CreateWindow("Isolated native rendering",1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    auto context=SDL_GL_CreateContext(window);
    if(!context || !bozo::render::gl::load([](const char *s)->void*{return reinterpret_cast<void*>(SDL_GL_GetProcAddress(s));}))return 2;
    auto &renderer=bozo::render::Renderer::instance();renderer.setDrawDigest(false);
    const std::vector<int> samples=door?std::vector<int>{448,720,1017,1080}:std::vector<int>{0,45,90,135,180,225,270,315};
    for(int yaw:samples){
        const int width=door?int(std::round(yaw*4.0/3.0)):640,height=door?yaw:224;
        bozo::host::FreeOptions options;options.elf="game/SCUS_971.11";options.sound=false;options.stallFields=20;
        bozo::host::FreeHost host(options);bozo::ee::Dispatcher d(host);
        d.setFunctions(dc_function_table,dc_function_count,dc_resume_table,dc_resume_count);
        bozo::port::Registry registry;dc::install(d.runtime(),registry);registry.registerFunction(done,finish);registry.registerFunction(done+4,doorBoard);
        std::vector<BozoExternalEntry> externals(dc_external_table,dc_external_table+dc_external_count);
        externals.push_back({done,1});externals.push_back({done+4,1});std::string error;
        if(!host.setUp(d,externals.data(),externals.size(),&registry,error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
        auto *r=d.runtime().memory.flat();std::memcpy(r,snapshot.data(),snapshot.size());
        float view[16],rotated[16];std::memcpy(view,r+info+0x10,sizeof(view));std::memcpy(rotated,view,sizeof(view));
        const float angle=yaw*3.14159265358979323846f/180,co=std::cos(angle),si=std::sin(angle);
        // world-to-view multiplied by a world-space yaw. Position stays fixed.
        for(int row=0;row<4;++row){rotated[row]=view[row]*co-view[8+row]*si;rotated[8+row]=view[row]*si+view[8+row]*co;}
        float position[4];std::memcpy(position,r+word(r,gp-0x6f00)+0x260,sizeof(position));
        for(int row=0;row<4;++row)for(int axis=0;axis<3;++axis)
            rotated[12+row]+=(view[axis*4+row]-rotated[axis*4+row])*position[axis];
        std::memcpy(r+info+0x10,rotated,sizeof(rotated));
        d.boot(door?done+4:0x187f90,0x1fff000);auto &c=d.kernel().thread(1)->frames.front()->ctx;
        c.r[28].uw[0]=gp;c.r[31].uw[0]=done;c.r[4].uw[0]=0x1d35730;c.r[5].uw[0]=0x1d35740;
        c.r[6].uw[0]=word(r,gp-0x6ef8);c.r[7].uw[0]=word(r,gp-0x6f00);
        c.r[8].uw[0]=word(r,gp-0x6fa0)+0x1be0;c.fu[12]=std::bit_cast<uint32_t>(clock);
        renderer.takeFrameDraws();renderer.beginFrame({1728,1936,640,224});
        try{d.run();}catch(const bozo::ee::EndRun &){}
        if(!host.endReason().empty()){std::fprintf(stderr,"%s\n",host.endReason().c_str());return 1;}
        renderer.endFrame();const auto frame=renderer.published();
        std::printf("time %.4f yaw %d: %zu batches %zu vertices\n",clock,yaw,frame->batches.size(),frame->vertices.size());
        for(const auto &b:frame->batches){
            uint32_t low[4]={255,255,255,255},high[4]={};
            for(uint32_t i=b.first;i<b.first+b.count;++i)for(int k=0;k<4;++k){const uint32_t v=(frame->vertices[i].rgba>>(k*8))&255;low[k]=std::min(low[k],v);high[k]=std::max(high[k],v);}
            std::printf("  count %u texture %llx fog %d depth %d/%d blend %d RGBA %u,%u,%u,%u .. %u,%u,%u,%u\n",b.count,(unsigned long long)(b.texture?b.texture->key:0),b.material.fog,b.material.depthTest,b.material.depthWrite,int(b.material.blend),low[0],low[1],low[2],low[3],high[0],high[1],high[2],high[3]);
        }
        bozo::render::gl::api().Viewport(0,0,width,height);
        if(!renderer.present(0,0,width,height))return 1;
        std::vector<uint8_t> pixels(width*height*4),flipped(pixels.size());
        bozo::render::gl::api().ReadPixels(0,0,width,height,bozo::render::gl::kRgba,bozo::render::gl::kUnsignedByte,pixels.data());
        for(int y=0;y<height;++y)std::memcpy(flipped.data()+y*width*4,pixels.data()+(height-1-y)*width*4,width*4);
        const auto path=std::filesystem::path(argv[2])/((door?"door-":"yaw-")+std::to_string(yaw)+".png");
        bozo::app::writePng(path.string(),flipped.data(),width,height);
    }
    renderer.releaseGpu();SDL_GL_DestroyContext(context);SDL_DestroyWindow(window);SDL_Quit();
}
