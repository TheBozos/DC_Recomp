// Native page controls and original Options lifecycle, with synthetic font and
// player state. Real cheat resource hooks consume the queued weapon edit.
#include "../runtime/dc_ingame_cheats.h"
#include "../runtime/dc_cheats.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/render/renderer.h"
#include <cstdio>
#include <cstring>
#include <memory>

namespace video=bozo::app::video;
namespace {
int postCalls=0;video::Settings posted;bool postedSave=false;
video::Shared fakeWindow(){
    video::Shared s;s.open=true;s.applied.width=1280;s.applied.height=960;s.applied.mode=video::WindowMode::Windowed;
    s.applied.renderScale=1.f;s.applied.frameRate=0;s.resolutions={{1280,960},{1920,1080}};s.monitor={2560,1440};s.monitorRefresh=144;
    return s;
}
void fakePost(const video::Settings &settings,bool save){++postCalls;posted=settings;postedSave=save;}
constexpr uint32_t initPc=0x21e4d0,exitPc=0x21e780,keyPc=0x21e9b0,drawPc=0x21f2b0;
constexpr uint32_t phase=0x1da8a50,fade=0x1da8a54,loaded=0x1da8ac4,pad=0x1cbc540;
constexpr uint32_t user=0x400000,gp=0x300000,weapon=user+0x450c,record=0x1c76968,pixels=0x500000;
int failures=0,initCalls=0,exitCalls=0,keyCalls=0,drawCalls=0,sceneCalls=0;
bozo::port::Registry *ports=nullptr;
template<class T>T read(const uint8_t *r,uint32_t p){T v;std::memcpy(&v,r+p,sizeof(v));return v;}
template<class T>void write(uint8_t *r,uint32_t p,T v){std::memcpy(r+p,&v,sizeof(v));}
void check(bool ok,const char *s){if(!ok){++failures;std::printf("FAIL %s\n",s);}}
void ret(R5900Context *c,int v=0){setReturnS32(c,v);c->pc=getRegU32(c,31);}
void originalInit(uint8_t *r,R5900Context *c,Runtime *){++initCalls;write<int32_t>(r,phase,1);write<int32_t>(r,fade,0);write<uint16_t>(r,loaded,1);ret(c,1);}
void originalExit(uint8_t *,R5900Context *c,Runtime *){++exitCalls;ret(c);}
void originalKey(uint8_t *r,R5900Context *c,Runtime *rt){
    ++keyCalls;const int mode=read<int32_t>(r,phase);int result=0;
    if(mode==1 && read<int32_t>(r,fade)>=13){write<int32_t>(r,phase,0);write<int32_t>(r,fade,0);}
    if(mode==0){
        const uint32_t pressed=read<uint32_t>(r,pad+4)&~read<uint32_t>(r,pad+0x9c);
        if(pressed&0x2000)write<int32_t>(r,0x1da8a64,read<int32_t>(r,0x1da8a64)+1);
        if(pressed&0x20){write<int32_t>(r,phase,2);write<int32_t>(r,fade,0);}
    }
    if(mode==2 && read<int32_t>(r,fade)>=25){ports->find(exitPc)(r,c,rt);result=1;}
    ret(c,result);
}
void originalDraw(uint8_t *r,R5900Context *c,Runtime *){++drawCalls;if(read<int32_t>(r,phase))write<int32_t>(r,fade,read<int32_t>(r,fade)+1);ret(c);}
void resource(uint8_t *,R5900Context *c,Runtime *){ret(c);}
void prepareScene(){++sceneCalls;}
void call(Runtime &rt,bozo::port::Registry &p,uint32_t pc){
    R5900Context c;c.pc=pc;c.r[31].uw[0]=0x123450;c.r[28].uw[0]=gp;c.r[16].ud[0]=0x9988776655443322ull;
    p.find(pc)(rt.memory.ram(),&c,&rt);
    check(c.pc==0x123450 && c.r[16].ud[0]==0x9988776655443322ull,"menu returns normally without clobbering saved CPU state");
}
void tap(Runtime &rt,bozo::port::Registry &p,uint32_t button){
    auto *r=rt.memory.ram();write<uint32_t>(r,pad+4,button);write<uint32_t>(r,pad+0x9c,0);call(rt,p,keyPc);
    check(read<uint32_t>(r,pad+4)==button && read<uint32_t>(r,pad+0x9c)==0,"menu leaves pad memory unchanged");
    write<uint32_t>(r,pad+4,0);write<uint32_t>(r,pad+0x9c,button);call(rt,p,keyPc);
}
void step(Runtime &rt,bozo::port::Registry &p){R5900Context c;c.pc=0x1bea50;c.r[4].uw[0]=user;c.r[28].uw[0]=gp;c.r[31].uw[0]=0x123450;p.find(c.pc)(rt.memory.ram(),&c,&rt);}
void draw(Runtime &rt,bozo::port::Registry &p){auto &render=bozo::render::Renderer::instance();render.beginFrame({1728,1936,640,224});call(rt,p,drawPc);render.endFrame();}
void finishFade(Runtime &rt,bozo::port::Registry &p){const int before=exitCalls;for(int i=0;i<27 && exitCalls==before;++i){draw(rt,p);call(rt,p,keyPc);}}
}
int main(){
    auto rt=std::make_unique<Runtime>();bozo::port::Registry p;ports=&p;
    const BozoFunctionEntry table[]={{0x1b5d90,resource},{0x1be510,resource},{0x1be710,resource},{0x1be900,resource},{0x1bea50,resource},
        {initPc,originalInit},{exitPc,originalExit},{keyPc,originalKey},{drawPc,originalDraw}};
    rt->functions.setGenerated(table,9);dc::cheats::install(*rt,p);dc::cheats::installIngameMenu(*rt,p,prepareScene);
    auto *r=rt->memory.ram();write<uint32_t>(r,gp-0x6388,user);write<int16_t>(r,weapon,1);write<int16_t>(r,weapon+4,30);
    write<int8_t>(r,user+0x4360,50);write<uint16_t>(r,user+0x4346,1000);
    for(int i=0;i<103;++i)write<int16_t>(r,user+0x436eu+i*2,-1);
    for(int i=0;i<3;++i)write<int16_t>(r,user+0x4362+i*2,-1);
    write<int16_t>(r,user+0x4362,135);write<int16_t>(r,user+0x4368,3);
    write<uint32_t>(r,record,128u<<16);write<uint32_t>(r,record+4,(3u<<16)|128u);write<uint32_t>(r,record+0x38,pixels);
    std::strcpy(reinterpret_cast<char *>(r+record+8),"ankfnt24");std::memset(r+pixels,255,128*128*3);
    for(int i=0;i<4;++i)dc::cheats::setEnabled(static_cast<dc::cheats::Cheat>(i),false);
    step(*rt,p);check(dc::cheats::equippedWeaponAttack()==30,"real cheat hook publishes equipped ATK");
    call(*rt,p,initPc);finishFade(*rt,p);
    check(initCalls==1 && read<int32_t>(r,phase)==0,"original loading and opening fade complete");
    check(drawCalls==0 && sceneCalls>0,"native page owns drawing after flushing scene work");
    const auto frame=bozo::render::Renderer::instance().published();check(frame && frame->batches.size()>=2,"root page submits board and owned font geometry");
    tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x40); // Root -> Graphics -> Cheats
    for(int i=0;i<4;++i){tap(*rt,p,0x40);check(dc::cheats::enabled(static_cast<dc::cheats::Cheat>(i)),"each cheat is controllable through the in-game page");if(i<3)tap(*rt,p,0x4000);}
    tap(*rt,p,0x4000); // ATK row
    tap(*rt,p,0x2000);tap(*rt,p,8);tap(*rt,p,2);tap(*rt,p,0x8000); // 30+1+10+100-1 = 140
    check(read<int16_t>(r,weapon+4)==30,"editing ATK does not modify the weapon before Apply");
    tap(*rt,p,0x4000);tap(*rt,p,0x40);check(read<int16_t>(r,weapon+4)==30,"Apply queues an edit until real gameplay resumes");step(*rt,p);
    check(read<int16_t>(r,weapon+4)==140,"page shares the real equipped-weapon one-shot edit");
    write<int16_t>(r,weapon+4,41);step(*rt,p);check(read<int16_t>(r,weapon+4)==41,"ATK request is consumed once");
    tap(*rt,p,0x1000);for(int i=0;i<12;++i)tap(*rt,p,2);tap(*rt,p,0x4000);tap(*rt,p,0x40);step(*rt,p);
    check(read<int16_t>(r,weapon+4)==999,"controller ATK changes clamp at 999");
    tap(*rt,p,0x1000);for(int i=0;i<12;++i)tap(*rt,p,1);tap(*rt,p,0x4000);tap(*rt,p,0x40);step(*rt,p);
    check(read<int16_t>(r,weapon+4)==0,"controller ATK changes clamp at zero");
    write<int8_t>(r,user+0x4340,-1);step(*rt,p);tap(*rt,p,0x40);write<int8_t>(r,user+0x4340,0);write<int16_t>(r,weapon+4,33);step(*rt,p);
    check(read<int16_t>(r,weapon+4)==33,"no equipped weapon prevents queuing an edit");
    tap(*rt,p,0x4000);tap(*rt,p,0x2000);check(dc::cheats::absMultiplier()==2,"controller exposes ABS multiplier after scrolling");
    tap(*rt,p,0x4000);tap(*rt,p,8);tap(*rt,p,0x4000);tap(*rt,p,0x40);
    check(read<uint16_t>(r,user+0x4346)==1000,"Gilda row queues rather than writes inventory directly");step(*rt,p);
    check(read<uint16_t>(r,user+0x4346)==1100,"Gilda controller steps use 100 on R1 and apply once");
    tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,8);tap(*rt,p,0x4000);tap(*rt,p,0x40);step(*rt,p);
    check(read<int16_t>(r,user+0x4368)==13,"active consumable quantity is editable through controller page");
    tap(*rt,p,0x4000);tap(*rt,p,0x2000);check(dc::cheats::movementPercent()==105,"movement row changes by five percent");
    tap(*rt,p,0x4000);tap(*rt,p,0x40);check(dc::cheats::enabled(dc::cheats::Cheat::FreezeTimeOfDay),"lighting freeze toggle is controller accessible");
    tap(*rt,p,0x4000);tap(*rt,p,2);tap(*rt,p,0x4000);tap(*rt,p,0x40);step(*rt,p);
    check(read<int8_t>(r,user+0x4360)==100,"inventory size is controller accessible and capped at 100");
    draw(*rt,p);const auto scrolled=bozo::render::Renderer::instance().published();check(scrolled && scrolled->batches.size()>=2,"last scroll page draws native text and board");
    tap(*rt,p,0x20);tap(*rt,p,0x1000);tap(*rt,p,0x1000);tap(*rt,p,0x40); // back -> Game Options
    const int beforeDraw=drawCalls;draw(*rt,p);check(drawCalls==beforeDraw+1,"original Game Options drawing remains available");
    tap(*rt,p,0x2000);check(read<int32_t>(r,0x1da8a64)==1,"Game Options input reaches its original handler");
    tap(*rt,p,0x20);check(read<int32_t>(r,phase)==0,"back from Game Options returns to root without closing");
    const int beforeKey=keyCalls;tap(*rt,p,0x2020);check(read<int32_t>(r,0x1da8a64)==1,"root close cannot edit an invisible Game Options row");
    check(read<int32_t>(r,phase)==2 && keyCalls>=beforeKey,"root close starts the original closing phase");finishFade(*rt,p);
    check(exitCalls==1,"original exit runs after its closing fade");
    check(dc::cheats::enabled(dc::cheats::Cheat::InfiniteStamina),"closing Options preserves session cheat choices");
    call(*rt,p,initPc);finishFade(*rt,p);tap(*rt,p,0x1000);tap(*rt,p,0x40); // Root -> Quit
    check(!dc::cheats::takeIngameQuitRequest(),"opening Quit Game does not request shutdown");
    tap(*rt,p,0x40);check(!dc::cheats::takeIngameQuitRequest(),"quit confirmation defaults to Cancel");
    tap(*rt,p,0x40);tap(*rt,p,0x20);check(!dc::cheats::takeIngameQuitRequest(),"Circle cancels quit confirmation");
    tap(*rt,p,0x40);tap(*rt,p,0x4000);tap(*rt,p,0x40);
    check(dc::cheats::takeIngameQuitRequest() && !dc::cheats::takeIngameQuitRequest(),"explicit Quit Game confirmation produces one shutdown request");
    call(*rt,p,exitPc);
    // Graphics: without the window (headless) the page only explains; with it, Apply posts the edited settings once.
    call(*rt,p,initPc);finishFade(*rt,p);tap(*rt,p,0x4000);tap(*rt,p,0x40); // Root -> Graphics
    tap(*rt,p,0x2000);tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x40);
    draw(*rt,p);const auto unavailable=bozo::render::Renderer::instance().published();
    check(postCalls==0 && unavailable && unavailable->batches.size()>=2,"Graphics without a window draws its notice and posts nothing");
    tap(*rt,p,0x20);check(read<int32_t>(r,phase)==0,"back from Graphics returns to root without closing");
    dc::cheats::setGraphicsAccess(fakeWindow,fakePost);tap(*rt,p,0x40); // Root (still on Graphics) -> Graphics
    tap(*rt,p,0x2000);tap(*rt,p,0x4000);tap(*rt,p,0x2000);tap(*rt,p,0x4000);tap(*rt,p,0x2000);tap(*rt,p,0x4000);tap(*rt,p,0x2000);
    check(postCalls==0,"Graphics edits wait for Apply");
    draw(*rt,p);const auto graphics=bozo::render::Renderer::instance().published();check(graphics && graphics->batches.size()>=2,"Graphics page draws native text and board");
    tap(*rt,p,0x4000);tap(*rt,p,0x40);
    check(postCalls==1 && postedSave && posted.mode==video::WindowMode::Fullscreen && posted.width==1920 && posted.height==1080 &&
          posted.renderScale==1.5f && posted.frameRate==-1,"Apply posts mode, size, render scale and frame rate once, saved");
    tap(*rt,p,0x4000);tap(*rt,p,0x2000);tap(*rt,p,0x4000);tap(*rt,p,0x2000); // Display -> Borderless; its size row is locked
    tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x4000);tap(*rt,p,0x40);
    check(postCalls==2 && posted.mode==video::WindowMode::Borderless && posted.width==1280 && posted.height==960,
          "Borderless keeps the window size it returns to");
    const int exitsBefore=exitCalls;tap(*rt,p,0x20);tap(*rt,p,0x20);finishFade(*rt,p);check(exitCalls==exitsBefore+1,"Options still closes after Graphics");
    std::memset(r+record+8,0,32);call(*rt,p,initPc);finishFade(*rt,p);const int fallback=drawCalls;draw(*rt,p);
    check(drawCalls==fallback+1,"missing font falls back to original Options without trapping input");
    for(int i=0;i<4;++i)dc::cheats::setEnabled(static_cast<dc::cheats::Cheat>(i),false);
    std::printf("In-game cheats menu: %d failures\n",failures);return failures?1:0;
}
