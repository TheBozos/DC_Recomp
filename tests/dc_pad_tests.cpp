// The native controller (runtime/dc_pad.cpp): read_pad fills CGamePad's PAD_STATUS as the original did from scePadRead
// once its handshake ended, and Step's motor levels reach the controller's rumble.
#include "../runtime/dc_pad.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
int failures=0;
void check(bool ok,const char *what){if(!ok){++failures;std::printf("FAIL %s\n",what);}}
constexpr uint32_t pad=0x1cbc540,readPadPc=0x12a900,stepPc=0x12b140,ra=0x12ae00;
struct Controller : bozo::libs::PadInput {
    bozo::libs::PadInputState state[2];
    uint32_t lastFrame=0;int lastPlayer=-1,rumbles=0;bool small=false;uint8_t large=0;
    void read(uint32_t frame,int player,bozo::libs::PadInputState &out) override{lastFrame=frame;lastPlayer=player;out=state[player&1];}
    void rumble(int player,bool s,uint8_t l) override{++rumbles;lastPlayer=player;small=s;large=l;}
};
struct Host : bozo::port::Host {
    bozo::libs::Libraries &libs;
    explicit Host(bozo::libs::Libraries &l):libs(l){}
    bozo::libs::Libraries &libraries() override{return libs;}
    void frameEnd(Runtime *,R5900Context *) override{throw std::runtime_error("unexpected frame end");}
    bool callLibrary(Runtime *,R5900Context *,uint32_t) override{return false;}
};
uint32_t rd32(const uint8_t *r,uint32_t at){uint32_t v;std::memcpy(&v,r+at,4);return v;}
void wr32(uint8_t *r,uint32_t at,uint32_t v){std::memcpy(r+at,&v,4);}
void call(bozo::port::Registry &p,Runtime &rt,uint32_t pc,uint32_t a0,uint32_t a1=0){
    R5900Context c;c.pc=pc;c.r[4].uw[0]=a0;c.r[5].uw[0]=a1;c.r[6].uw[0]=0;c.r[31].uw[0]=ra;
    p.find(pc)(rt.memory.ram(),&c,&rt);check(c.pc==ra,"the native routine returns to its caller");
}
// What the original pad_button_read made of scePadRead's bytes (the engine writes the buttons active low, low byte first).
uint32_t originalButtons(uint16_t pressed){
    const uint16_t released=static_cast<uint16_t>(~pressed);const uint8_t d2=released&0xff,d3=released>>8;
    return static_cast<uint32_t>(((d2<<8)|d3)^0xffff);
}
}

int main(){
    auto rt=std::make_unique<Runtime>();bozo::libs::Libraries libs;Host host(libs);rt->port=&host;
    Controller controller;libs.pad.input=&controller;libs.framesEnded=42;
    bozo::port::Registry p;dc::pad::install(p);
    check(p.find(readPadPc) && p.find(stepPc),"read_pad and Step are native");
    auto *r=rt->memory.ram();

    for(int bit=0;bit<16;++bit){
        controller.state[0]={};controller.state[0].connected=true;controller.state[0].buttons=static_cast<uint16_t>(1u<<bit);
        call(p,*rt,readPadPc,pad+4,0);
        check(rd32(r,pad+4)==originalButtons(static_cast<uint16_t>(1u<<bit)),"each button lands where scePadRead put it");
    }
    controller.state[0]={};controller.state[0].buttons=bozo::libs::kPadCross|bozo::libs::kPadUp;
    controller.state[0].rx=1;controller.state[0].ry=2;controller.state[0].lx=3;controller.state[0].ly=4;
    call(p,*rt,readPadPc,pad+4,0);
    check(rd32(r,pad+4)==0x1040u,"Cross and Up are DC's 0x40 and 0x1000");
    check(rd32(r,pad+8)==4 && rd32(r,pad+12)==3 && rd32(r,pad+16)==2 && rd32(r,pad+20)==1,"sticks in ly, lx, ry, rx order");
    check(rd32(r,pad+24)==99 && rd32(r,pad+28)==6 && rd32(r,pad+32)==7 && rd32(r,pad+36)==7 && rd32(r,pad+40)==7,
          "the handshake reads as ended: ready, stable, DualShock 2");
    check(r[pad+4+0x2e]==0 && r[pad+4+0x2f]==1 && r[pad+4+0x30]==0xff && r[pad+4+0x33]==0xff,"motor alignment as the handshake set it");
    check(controller.lastFrame==42 && controller.lastPlayer==0,"port 0 reads player 1 at the frames ended");

    controller.state[1]={};controller.state[1].buttons=bozo::libs::kPadStart;controller.state[1].lx=0x10;
    call(p,*rt,readPadPc,pad+0x50,1);
    check(controller.lastPlayer==1 && rd32(r,pad+0x50)==originalButtons(bozo::libs::kPadStart) && rd32(r,pad+0x58)==0x10,
          "port 1 reads player 2 into its own status");
    check(rd32(r,pad+4)==0x1040u,"port 1 leaves port 0's status alone");

    libs.pad.input=nullptr;call(p,*rt,readPadPc,pad+4,0);
    check(rd32(r,pad+4)==0 && rd32(r,pad+8)==0x80 && rd32(r,pad+20)==0x80,"no input source: nothing pressed, sticks centred");
    libs.pad.input=&controller;

    // Step: vibration enabled, port 0's small motor for two more frames, the large one's timer ran out.
    wr32(r,pad+0x464,1);r[pad+0x2c]=1;r[pad+0x2d]=200;wr32(r,pad+0x38,2);wr32(r,pad+0x3c,0);
    r[pad+0x4c+0x2c]=1;wr32(r,pad+0x4c+0x38,5);
    controller.rumbles=0;call(p,*rt,stepPc,pad);
    check(rd32(r,pad+0x38)==1 && r[pad+0x2c]==1 && rd32(r,pad+0x3c)==0 && r[pad+0x2d]==0,"motor timers count down and expire");
    check(rd32(r,pad+0x4c+0x38)==4 && r[pad+0x4c+0x2c]==1,"port 1's timers count down too");
    check(controller.rumbles==1 && controller.lastPlayer==0 && controller.small && controller.large==0,"port 0's levels reach player 1's rumble");
    wr32(r,pad+0x464,0);r[pad+0x2d]=90;wr32(r,pad+0x3c,9);call(p,*rt,stepPc,pad);
    check(r[pad+0x2c]==0 && r[pad+0x2d]==0 && !controller.small && controller.large==0,"vibration disabled stops both motors");

    std::printf("Native pad: %d failures\n",failures);return failures?1:0;
}
