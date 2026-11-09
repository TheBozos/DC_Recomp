// Dark Cloud's controller, native. CGamePad (the global at 0x1CBC540) keeps one 19-word PAD_STATUS per port at +4 and
// +0x50: buttons, ly, lx, ry, rx, then the libpad handshake's phase, state, mode and mode IDs, and the vibration bytes
// at +0x28 (the levels Step sends) and +0x2E (the actuator alignment). The original read_pad (0x12A900) walked the
// libpad handshake (scePadGetState, scePadInfoMode, scePadSetMainMode, scePadInfoAct, scePadSetActAlign) before its
// pad_button_read (0x12A830) called scePadRead; Step (0x12B140) sent the levels with scePadSetActDirect. Here the
// controller in each port is what it always is on the PC, a DualShock 2 in analog mode: its state comes from the
// engine's PadInput (the window's devices, or a script in checks) and its rumble goes back to it. CGamePad::Init's
// scePadInit and scePadPortOpen calls are retired by guarded patches (recomp/dc_native.patches).
#include "dc_pad.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <cstring>

namespace dc::pad {
namespace {
constexpr uint32_t kReadPad=0x12a900,kStep=0x12b140;
// The handshake's end as the original leaves it: phase 99 (ready), state 6 (stable), mode IDs 7 (DualShock 2).
constexpr uint32_t kReady=99,kStable=6,kDualShock=7;

uint32_t rd32(const uint8_t *ram,uint32_t at){uint32_t v;std::memcpy(&v,ram+(at&0x1ffffffu),4);return v;}
void wr32(uint8_t *ram,uint32_t at,uint32_t v){std::memcpy(ram+(at&0x1ffffffu),&v,4);}
void wr8(uint8_t *ram,uint32_t at,uint8_t v){ram[at&0x1ffffffu]=v;}
void returnVoid(R5900Context *c){c->pc=c->r[31].uw[0];}

// read_pad(PAD_STATUS *status, int port, int slot): CGamePad::UpDate calls it for port 0 and port 1 (slot 0).
void readPad(uint8_t *ram,R5900Context *c,Runtime *rt)
{
    const uint32_t status=c->r[4].uw[0];
    const int port=static_cast<int>(c->r[5].uw[0]);
    bozo::libs::PadInputState input;
    if(rt->port && (port==0 || port==1)){
        auto &libraries=rt->port->libraries();
        if(libraries.pad.input)libraries.pad.input->read(libraries.framesEnded,port,input);
    }
    // scePadRead's halfword arrives byte-swapped in DC's word: Cross is 0x40, Up 0x1000.
    const uint16_t buttons=static_cast<uint16_t>((input.buttons<<8)|(input.buttons>>8));
    wr32(ram,status,buttons);
    wr32(ram,status+4,input.ly);wr32(ram,status+8,input.lx);wr32(ram,status+12,input.ry);wr32(ram,status+16,input.rx);
    wr32(ram,status+20,kReady);wr32(ram,status+24,kStable);wr32(ram,status+28,kDualShock);
    wr32(ram,status+32,kDualShock);wr32(ram,status+36,kDualShock);
    // The alignment the handshake set: byte 0 the small motor, byte 1 the large one, the rest none.
    wr8(ram,status+0x2e,0);wr8(ram,status+0x2f,1);for(uint32_t i=2;i<6;++i)wr8(ram,status + 0x2e + i,0xff);
    returnVoid(c);
}

// CGamePad::Step(): each port's two motor timers count down and a motor whose timer ran out stops (both stop while
// vibration is disabled at +0x464); port 0's levels then go to player 1's controller.
void step(uint8_t *ram,R5900Context *c,Runtime *rt)
{
    const uint32_t pad=c->r[4].uw[0];
    for(uint32_t port=0;port<2;++port){
        const uint32_t base=pad+port*0x4c;
        if(rd32(ram,pad+0x464)==0){wr8(ram,base+0x2c,0);wr8(ram,base+0x2d,0);}
        for(uint32_t motor=0;motor<2;++motor){
            const uint32_t timer=base+0x38+motor*4;
            const int32_t left=static_cast<int32_t>(rd32(ram,timer));
            if(left<1){wr32(ram,timer,0);wr8(ram,base+0x2c+motor,0);}else wr32(ram,timer,static_cast<uint32_t>(left-1));
        }
    }
    if(rt->port){
        auto &libraries=rt->port->libraries();
        if(libraries.pad.input)libraries.pad.input->rumble(0,ram[(pad+0x2c)&0x1ffffffu]!=0,ram[(pad+0x2d)&0x1ffffffu]);
    }
    returnVoid(c);
}
}

void install(bozo::port::Registry &registry)
{
    registry.registerFunction(kReadPad,readPad);
    registry.registerFunction(kStep,step);
}
}
