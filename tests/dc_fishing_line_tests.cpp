// Disc-free native fishing boundary, including rejected/degenerate segments,
// signed coordinates, call resets and invalid entry state.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
namespace dc { bool nativeInstruction(uint8_t *,R5900Context *,Runtime *,uint32_t); }
namespace
{
    int failures=0;
    void check(bool ok,const char *s) { if (!ok) { ++failures; std::printf("FAIL %s\n",s); } }
    void point(Runtime &rt,uint32_t index,uint32_t decision,int32_t x,int32_t y,uint32_t z)
    {
        R5900Context ctx; ctx.r[29].uw[0]=0x400000; ctx.r[19].uw[0]=index; ctx.r[5].uw[0]=decision;
        const uint32_t xyz[3]={static_cast<uint32_t>(x),static_cast<uint32_t>(y),z};
        std::memcpy(rt.memory.ram()+0x400070,xyz,12);
        const auto before=ctx;
        check(dc::nativeInstruction(rt.memory.ram(),&ctx,&rt,0x1ab290),"declared native site handles the point");
        check(std::memcmp(&ctx,&before,sizeof(ctx))==0,"point submission preserves all registers and control flow");
    }
}
int main()
{
    auto rt=std::make_unique<Runtime>(); auto &renderer=bozo::render::Renderer::instance();
    renderer.setEnabled(true); renderer.beginFrame({});
    point(*rt,0,4,16,32,123); point(*rt,1,12,-16,-32,234);
    point(*rt,2,4,16,-32,345); point(*rt,3,4,16,-32,456);
    point(*rt,4,12,160,320,567); point(*rt,5,4,160,352,0xabcdef12);
    const std::vector<uint8_t> before(rt->memory.ram(),rt->memory.ram()+bozo::kFlatScratchpadOffset);
    renderer.endFrame(); auto frame=renderer.published();
    check(frame && frame->batches.size()==2 && frame->vertices.size()==12,"only the two nondegenerate accepted connections draw");
    if (frame && frame->vertices.size()==12)
    {
        const auto &v=frame->vertices;
        check((v[0].x+v[1].x)*0.5f==-1 && (v[0].y+v[1].y)*0.5f==-2 && v[0].z==234,
            "rejected point becomes the next endpoint with signed coordinates intact");
        check(v[10].z==0xcdef12 && v[10].rgba==0x80404040,"native depth range and fishing line color are explicit");
    }
    renderer.beginFrame({}); point(*rt,0,4,1000,2000,12); point(*rt,1,4,1016,2000,34); renderer.endFrame();
    frame=renderer.published();
    check(frame && frame->batches.size()==1,"new call resets the previous endpoint");
    // Invalid state must fail before reading point/packet memory or publishing geometry.
    R5900Context ctx; ctx.r[19].uw[0]=24; ctx.r[5].uw[0]=4; ctx.r[29].uw[0]=0x12000000;
    bool rejected=false;
    try { dc::nativeInstruction(rt->memory.ram(),&ctx,rt.get(),0x1ab290); } catch (const std::runtime_error &) { rejected=true; }
    check(rejected,"out-of-range point fails explicitly");
    ctx.r[19].uw[0]=2; ctx.r[5].uw[0]=8; rejected=false;
    try { dc::nativeInstruction(rt->memory.ram(),&ctx,rt.get(),0x1ab290); } catch (const std::runtime_error &) { rejected=true; }
    check(rejected,"unknown connection decision fails explicitly");
    // Only the caller-supplied point changed since the snapshot; native drawing writes no RAM.
    auto expected=before; std::memcpy(expected.data()+0x400070,rt->memory.ram()+0x400070,12);
    check(std::memcmp(rt->memory.ram(),expected.data(),expected.size())==0,"native geometry leaves all RAM unchanged");
    std::printf("DC native fishing boundary: %d failures\n",failures); return failures ? 1 : 0;
}
