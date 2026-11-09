// Controller-operated pages inside the original Options lifecycle. No guest
// text buffers, menu-table extensions, save-format changes or console packets.
#include "dc_ingame_cheats.h"
#include "dc_cheats.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/app/video_settings.h"
#include "bozo/render/renderer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dc::cheats {
namespace {
constexpr uint32_t kInit=0x21e4d0,kExit=0x21e780,kKey=0x21e9b0,kDraw=0x21f2b0;
constexpr uint32_t kPhase=0x1da8a50,kFade=0x1da8a54,kLoaded=0x1da8ac4,kPad=0x1cbc540;
constexpr uint32_t kUp=0x1000,kDown=0x4000,kLeft=0x8000,kRight=0x2000,kCross=0x40,kCircle=0x20;
constexpr uint32_t kL1=4,kR1=8,kL2=1,kR2=2;
constexpr float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
enum class Page { Root, Game, Graphics, Cheats, Quit };
struct Menu {
    bool active=false,queued=false;
    Page page=Page::Root;
    int root=0,row=0,attack=0,quitChoice=0,gold=0,slot=0,quantity=1,capacity=50,spawn=0,spawnCount=1;
    uint32_t held=0,ticks=0;
    std::shared_ptr<const bozo::render::Texture> font;
    // Graphics: the window's settings as edited on the page, posted to the window's thread by its Apply row.
    int graphicsRow=0;
    bozo::app::video::Settings graphics;
    const char *graphicsStatus="";
} menu;
constexpr int rootCount=4;
const char *const rootLabels[rootCount]={"Game Options","Graphics","Cheats","Quit Game"};
enum GraphicsRow { Display, Resolution, RenderScale, FrameRate, ApplyGraphics, GraphicsRowEnd };
constexpr float renderScales[]={0.5f,0.75f,1.f,1.5f,2.f};
RecompiledFunction originalInit=nullptr,originalExit=nullptr,originalKey=nullptr,originalDraw=nullptr;
RecompiledFunction originalHelp=nullptr,originalMessage=nullptr;
constexpr uint32_t kHelp=0x217A60,kMessage=0x153310;
// Rows 0..15 as before; then the level-up, map, fishing, spawner and fast-forward rows.
enum Row { LevelUp=16, RevealMap, FishBite, SpawnItem, SpawnCount, GiveItem, FastSpeed, FastLock, DamageRate, OneHit,
           NoDamage, KeyFinder, RowEnd };
constexpr int rowCount=RowEnd;
// The toggle rows and the cheat each switches.
bool toggleRow(int row,Cheat &cheat)
{
    if(row<4){cheat=static_cast<Cheat>(row);return true;}
    if(row==13){cheat=Cheat::FreezeTimeOfDay;return true;}
    if(row==RevealMap){cheat=Cheat::RevealFloorMap;return true;}
    if(row==FishBite){cheat=Cheat::FishingHelper;return true;}
    if(row==FastLock){cheat=Cheat::FastForwardLock;return true;}
    if(row==OneHit){cheat=Cheat::OneHitKills;return true;}
    if(row==NoDamage){cheat=Cheat::NoDamageTaken;return true;}
    if(row==KeyFinder){cheat=Cheat::KeyFinder;return true;}
    return false;
}
void (*prepareScene)()=nullptr;
uint64_t fontVersion=0;
std::atomic<bool> quitRequested{false};
std::atomic<bozo::app::video::Shared (*)()> videoShared{nullptr};
std::atomic<void (*)(const bozo::app::video::Settings &,bool)> videoPost{nullptr};
bozo::app::video::Shared windowSettings(){const auto f=videoShared.load(std::memory_order_acquire);return f?f():bozo::app::video::Shared{};}
template<class T> T read(const uint8_t *r,uint32_t at){T v;std::memcpy(&v,r+at,sizeof(v));return v;}
template<class T> void write(uint8_t *r,uint32_t at,T v){std::memcpy(r+at,&v,sizeof(v));}
void done(R5900Context *c){setReturnS32(c,0);c->pc=getRegU32(c,31);}

std::shared_ptr<const bozo::render::Texture> fontFromGame(const uint8_t *ram)
{
    const int last=std::clamp(read<int32_t>(ram,0x1c75870),-1,1023);
    for(int i=last;i>=0;--i){
        const uint32_t record=0x1c76968+uint32_t(i)*80;
        if(std::memcmp(ram+record+8,"ankfnt24",9)!=0)continue;
        const uint32_t w=read<uint32_t>(ram,record)>>16,h=read<uint32_t>(ram,record+4)&65535;
        const uint32_t kind=read<uint32_t>(ram,record+4)>>16,p=read<uint32_t>(ram,record+0x38)&0x1ffffff;
        if(w!=128 || h!=128 || (kind!=3 && kind!=4) || !p || p>0x2000000-w*h*kind || read<uint32_t>(ram,record+0x4c))return {};
        auto font=std::make_shared<bozo::render::Texture>();font->width=160;font->height=144;
        font->key=0x4443434845415446ull;font->version=++fontVersion;font->rgba.resize(font->width*font->height);
        // The game's ASCII atlas is white coverage on black RGB. Turn that
        // coverage into alpha once per menu opening, retaining an owned image.
        for(uint32_t n=0;n<w*h;++n){
            const uint32_t at=p+n*kind;
            uint32_t coverage=std::max({ram[at],ram[at+1],ram[at+2]});
            if(kind==4)coverage=coverage*std::min(128u,uint32_t(ram[at+3]))/128;
            const uint32_t x=n%w,y=n/w,glyphX=x/8,glyphY=y/16;
            const uint32_t destination=(glyphY*18+1+y%16)*font->width+glyphX*10+1+x%8;
            font->rgba[destination]=0x00ffffffu|((coverage*128+127)/255)<<24;
        }
        return font;
    }
    return {};
}
void quad(std::vector<bozo::render::Vertex> &v,float x,float y,float w,float h,uint32_t rgba,
          float u0=0,float v0=0,float u1=1,float v1=1)
{
    auto point=[&](float px,float py,float u,float t){bozo::render::Vertex a;a.x=1728+px;a.y=1936+py*.5f;a.u=u;a.v=t;a.rgba=rgba;return a;};
    const auto a=point(x,y,u0,v0),b=point(x+w,y,u1,v0),c=point(x,y+h,u0,v1),d=point(x+w,y+h,u1,v1);
    v.insert(v.end(),{a,b,c,b,d,c});
}
uint32_t colour(uint32_t rgb,int alpha){return (rgb&0xffffffu)|(uint32_t(std::clamp(alpha,0,128))<<24);}
void panel(int alpha)
{
    bozo::render::Material m;m.textured=false;m.directColour=true;m.depthTest=m.depthWrite=false;
    std::vector<bozo::render::Vertex> v;
    quad(v,96,54,448,344,colour(0x141216,alpha));
    quad(v,96,54,448,2,colour(0x7ca9d4,alpha));quad(v,96,396,448,2,colour(0x7ca9d4,alpha));
    quad(v,96,54,2,344,colour(0x7ca9d4,alpha));quad(v,542,54,2,344,colour(0x7ca9d4,alpha));
    bozo::render::Renderer::instance().draw(m,identity,v.data(),uint32_t(v.size()));
}
void text(std::vector<bozo::render::Vertex> &v,std::string_view s,float x,float y,float scale,uint32_t rgba)
{
    const float start=x;
    for(unsigned char ch:s){
        if(ch=='\n'){x=start;y+=18*scale;continue;}
        if(ch>=33 && ch<127){
            const unsigned glyph=ch-33;const float u=float((glyph&15)*10+1),t=float((glyph>>4)*18+1),bias=1.f/32;
            quad(v,x,y,8*scale,16*scale,rgba,(u+bias)/160,(t+bias)/144,(u+8+bias)/160,(t+16+bias)/144);
        }
        x+=8*scale;
    }
}
namespace video=bozo::app::video;
bool sameGraphics(const video::Settings &a,const video::Settings &b)
{
    return a.mode==b.mode && a.width==b.width && a.height==b.height && std::abs(a.renderScale-b.renderScale)<0.01f &&
           a.frameRate==b.frameRate;
}
int renderScaleIndex(float scale)
{
    int best=2;
    for(int i=0;i<int(std::size(renderScales));++i)if(std::abs(renderScales[i]-scale)<std::abs(renderScales[best]-scale))best=i;
    return best;
}
std::string graphicsValue(int row,const video::Settings &g,const video::Shared &shared)
{
    char text[64]="";
    if(row==Display)return g.mode==video::WindowMode::Fullscreen?"Fullscreen":g.mode==video::WindowMode::Borderless?"Borderless":"Windowed";
    if(row==Resolution){
        if(g.mode==video::WindowMode::Borderless)std::snprintf(text,sizeof(text),"%dx%d monitor",shared.monitor.first,shared.monitor.second);
        else std::snprintf(text,sizeof(text),"%dx%d",g.width,g.height);
    }
    if(row==RenderScale)std::snprintf(text,sizeof(text),"%d%%",int(std::lround(g.renderScale*100)));
    if(row==FrameRate){
        // DC's font has no parentheses (they draw as braces).
        if(g.frameRate==0)return "60 fps";
        if(g.frameRate<0)std::snprintf(text,sizeof(text),"Monitor %d Hz",shared.monitorRefresh);
        else std::snprintf(text,sizeof(text),"%d fps",g.frameRate);
    }
    return text;
}
// Left/Right on a graphics row: the next choice that way.
void stepGraphics(int row,int way,const video::Shared &shared)
{
    auto &g=menu.graphics;
    if(row==Display)g.mode=static_cast<video::WindowMode>((int(g.mode)+3+way)%3);
    else if(row==Resolution && g.mode!=video::WindowMode::Borderless && !shared.resolutions.empty()){
        const auto &sizes=shared.resolutions;const int n=int(sizes.size());
        int at=-1;for(int i=0;i<n;++i)if(sizes[i]==std::pair{g.width,g.height})at=i;
        at=at<0?(way>0?0:n-1):std::clamp(at+way,0,n-1);
        g.width=sizes[at].first;g.height=sizes[at].second;
    }else if(row==RenderScale)g.renderScale=renderScales[std::clamp(renderScaleIndex(g.renderScale)+way,0,int(std::size(renderScales))-1)];
    else if(row==FrameRate){
        std::vector<int> rates={0,-1};
        if(shared.applied.frameRate>0)rates.push_back(shared.applied.frameRate);
        int at=0;for(int i=0;i<int(rates.size());++i)if(rates[i]==g.frameRate)at=i;
        g.frameRate=rates[(at+int(rates.size())+way)%int(rates.size())];
    }
    menu.graphicsStatus="";
}
void drawGraphics(std::vector<bozo::render::Vertex> &v,uint32_t white,uint32_t gold,uint32_t dim)
{
    const video::Shared shared=windowSettings();
    if(!shared.open){
        text(v,"Window settings are not",156,170,1.25f,white);text(v,"available in this run.",156,200,1.25f,white);
        text(v,"O: Back",292,352,1,white);return;
    }
    const char *labels[]={"Display","Resolution","Render scale","Frame rate","Apply and save"};
    for(int i=0;i<GraphicsRowEnd;++i){
        const float y=120.f+i*34+(i==ApplyGraphics?12:0);
        const bool locked=i==Resolution && menu.graphics.mode==video::WindowMode::Borderless;
        const auto c=i==menu.graphicsRow?gold:locked?dim:white;
        text(v,labels[i],140,y,1.25f,c);if(i==menu.graphicsRow)text(v,">",120,y,1.25f,c);
        if(i==ApplyGraphics)continue;
        std::string value=graphicsValue(i,menu.graphics,shared);
        if(i==menu.graphicsRow && !locked)value="< "+value+" >";
        text(v,value,526-float(value.size())*10,y,1.25f,c);
    }
    const std::string now="Now: "+graphicsValue(Display,shared.applied,shared)+", "+graphicsValue(Resolution,shared.applied,shared)+
                          ", "+graphicsValue(RenderScale,shared.applied,shared)+", "+graphicsValue(FrameRate,shared.applied,shared);
    text(v,now,320-float(now.size())*3.6f,312,0.9f,dim);
    const char *help=menu.graphicsRow==Display?"L/R: Window, full screen or borderless":
                     menu.graphicsRow==Resolution?(menu.graphics.mode==video::WindowMode::Borderless?"Borderless uses the monitor size":"L/R: Window or full-screen size"):
                     menu.graphicsRow==RenderScale?"L/R: 3D resolution against the window":
                     menu.graphicsRow==FrameRate?"L/R: Monitor smooths between frames":"X: Apply now and keep for next launch";
    if(*menu.graphicsStatus)help=menu.graphicsStatus;
    else if(!sameGraphics(menu.graphics,shared.applied) && menu.graphicsRow!=ApplyGraphics)help="Not applied yet: choose Apply and save";
    text(v,help,320-float(std::strlen(help))*4,354,1,white);
}
void drawPage(int alpha)
{
    if(prepareScene)prepareScene();panel(alpha);
    std::vector<bozo::render::Vertex> v;
    const uint32_t white=colour(0x808080,alpha),gold=colour(0x207080,alpha),dim=colour(0x484848,alpha);
    const auto heading=menu.page==Page::Root?"OPTIONS":menu.page==Page::Quit?"QUIT GAME?":menu.page==Page::Graphics?"GRAPHICS":"CHEATS";
    text(v,heading,320-float(std::strlen(heading))*8,76,2,gold);
    if(menu.page==Page::Root){
        for(int i=0;i<rootCount;++i){const float y=136.f+i*44;const auto c=i==menu.root?gold:white;text(v,rootLabels[i],164,y,1.5f,c);if(i==menu.root)text(v,">",132,y,1.5f,c);}
        text(v,"D-pad: Select   X: Open   O: Back",120,352,1,white);
    }else if(menu.page==Page::Quit){
        text(v,"Close the game?",200,150,1.5f,white);
        text(v,"Unsaved progress will be lost.",134,192,1.15f,white);
        const char *labels[]={"Cancel","Quit Game"};
        for(int i=0;i<2;++i){const float y=240.f+i*38;const auto c=i==menu.quitChoice?gold:white;text(v,labels[i],204,y,1.5f,c);if(i==menu.quitChoice)text(v,">",172,y,1.5f,c);}
        text(v,"X: Confirm   O: Cancel",164,352,1,white);
    }else if(menu.page==Page::Graphics){
        drawGraphics(v,white,gold,dim);
    }else{
        const char *labels[]={"Infinite health","Weapon durability","Infinite thirst","Combo stamina",
            "Equipped weapon ATK","Apply weapon ATK","Weapon ABS multiplier","Gilda","Apply Gilda",
            "Active consumable slot","Consumable quantity","Apply quantity","Movement speed","Freeze time of day",
            "Inventory size","Apply inventory size","Level up equipped weapon","Reveal floor map","Fish always bite",
            "Spawn item","Spawn count","Give item","Fast-forward speed","Fast-forward always on","Damage multiplier",
            "One-hit kills","No damage taken","Key finder"};
        const int first=std::max(0,menu.row-5);
        for(int i=first;i<std::min(first+6,rowCount);++i){
            const float y=126.f+(i-first)*34;const auto c=i==menu.row?gold:white;
            text(v,labels[i],128,y,1.25f,c);if(i==menu.row)text(v,">",108,y,1.25f,c);
            std::string value;
            if(Cheat cheat;toggleRow(i,cheat))value=enabled(cheat)?"ON":"OFF";
            if(i==4)value=equippedWeaponAttack()<0?"---":std::to_string(menu.attack);
            if(i==6)value=std::to_string(absMultiplier())+"x";
            if(i==7)value=gilda()<0?"---":std::to_string(menu.gold);
            if(i==9)value=std::to_string(menu.slot+1);
            if(i==10)value=activeConsumable(menu.slot).id<0?"---":std::to_string(menu.quantity);
            if(i==12)value=std::to_string(movementPercent())+"%";
            if(i==14)value=inventorySize()<0?"---":std::to_string(menu.capacity);
            if(i==SpawnItem)value=spawnItemCount()?spawnItem(menu.spawn).name:"---";
            if(i==SpawnCount)value=std::to_string(menu.spawnCount);
            if(i==FastSpeed)value=std::to_string(fastForwardPercent())+"%";
            if(i==DamageRate)value=std::to_string(damageMultiplier())+"x";
            text(v,value,526-float(value.size())*10,y,1.25f,c);
        }
        char count[32];std::snprintf(count,sizeof(count),"%d/%d",menu.row+1,rowCount);text(v,count,470,88,1,dim);
        const char *help="L/R: 1  L1/R1: 10  L2/R2: 100  O: Back";
        if(Cheat cheat;toggleRow(menu.row,cheat))
            help=menu.row==RevealMap?"X: Toggle   Whole floor shown on the map":menu.row==FishBite?"X: Toggle   Fish bite and stay hooked":
                 menu.row==FastLock?"X: Toggle   Sound is muted while fast":
                 menu.row==OneHit?"X: Toggle   Any hit that deals damage kills":
                 menu.row==NoDamage?"X: Toggle   Monster attacks never land":
                 menu.row==KeyFinder?"X: Toggle   Key holder blinks gold on the map":"X: Toggle   Left/Right: Set   O: Back";
        else if(menu.row==6)help="L/R: 1x   Positive combat ABS   O: Back";
        else if(menu.row==7)help="L/R: 1  L1/R1: 100  L2/R2: 1000";
        else if(menu.row==9)help="L/R: Choose one of the three HUD slots";
        else if(menu.row==12)help="L/R: 5%   Collision stays enabled";
        else if(menu.row==14)help="1-100 slots; occupied items are protected";
        else if(menu.row==5 || menu.row==8 || menu.row==11 || menu.row==15)help="X: Apply when play resumes   O: Back";
        else if(menu.row==LevelUp)help="X: Fill ABS; level up in the weapon menu";
        else if(menu.row==SpawnItem)help=spawnItemCount()?"L/R: 1  L1/R1: 10   Story items excluded":"Item names load with the game";
        else if(menu.row==SpawnCount)help="L/R: 1   Each one takes a bag slot";
        else if(menu.row==GiveItem)help="X: Give when play resumes   O: Back";
        else if(menu.row==FastSpeed)help="L/R: 50%   Hold ` or the touchpad";
        else if(menu.row==DamageRate)help="L/R: 1x   Damage you deal to monsters";
        if((menu.row==4 || menu.row==5 || menu.row==LevelUp) && equippedWeaponAttack()<0)help="No weapon equipped   O: Back";
        if((menu.row==10 || menu.row==11) && activeConsumable(menu.slot).id<0)help="Equip a consumable in this HUD slot first";
        if(menu.queued)help="Edit queued; close Options to apply";
        text(v,help,112,354,1,white);

    }
    bozo::render::Material m;m.tex0=menu.font->key;m.nearest=true;m.depthTest=m.depthWrite=false;m.clampU=m.clampV=true;
    bozo::render::Renderer::instance().draw(m,identity,v.data(),uint32_t(v.size()),menu.font);
}
uint32_t keys(const uint8_t *r)
{
    // CGamePad::Down's actual boundary: blocked input, current and previous
    // buttons. Reading it leaves every game input structure unchanged.
    if(read<uint32_t>(r,kPad+0x45c)){menu.held=menu.ticks=0;return 0;}
    const uint32_t current=read<uint32_t>(r,kPad+4),previous=read<uint32_t>(r,kPad+0x9c);
    const uint32_t repeat=current&(kUp|kDown|kLeft|kRight|kL1|kR1|kL2|kR2);
    if(repeat && repeat==menu.held)++menu.ticks;else{menu.held=repeat;menu.ticks=0;}
    return (current&~previous)|((menu.ticks>=20 && menu.ticks%4==0)?repeat:0);
}
void init(uint8_t *r,R5900Context *c,Runtime *rt)
{
    refreshPlayer(r,c);menu={};menu.active=true;menu.font=fontFromGame(r);if(!menu.font)menu.page=Page::Game;
    originalInit(r,c,rt);
}
void exit(uint8_t *r,R5900Context *c,Runtime *rt){applyPlayerEdits(r,c);menu={};originalExit(r,c,rt);}
void key(uint8_t *r,R5900Context *c,Runtime *rt)
{
    if(!menu.active || !menu.font || read<int32_t>(r,kPhase)!=0){originalKey(r,c,rt);return;}
    const uint32_t pressed=keys(r);
    if(menu.page==Page::Game){
        if(pressed&kCircle){menu.page=Page::Root;menu.held=menu.ticks=0;done(c);}else originalKey(r,c,rt);
        return;
    }
    if(pressed&kCircle){
        if(menu.page==Page::Cheats || menu.page==Page::Graphics || menu.page==Page::Quit){menu.page=Page::Root;menu.held=menu.ticks=0;done(c);}
        else {
            // The original Circle boundary starts this fade. Do not also run
            // hidden Game Options navigation if Circle and a direction coincide.
            write<int32_t>(r,kPhase,2);write<int32_t>(r,kFade,0);done(c);
        }
        return;
    }
    if(menu.page==Page::Root){
        if(pressed&kUp)menu.root=(menu.root+rootCount-1)%rootCount;else if(pressed&kDown)menu.root=(menu.root+1)%rootCount;
        if(pressed&kCross){menu.page=menu.root==0?Page::Game:menu.root==1?Page::Graphics:menu.root==2?Page::Cheats:Page::Quit;
            menu.graphics=windowSettings().applied;menu.graphicsRow=0;menu.graphicsStatus="";menu.quitChoice=0;menu.held=menu.ticks=0;menu.attack=std::max(0,equippedWeaponAttack());menu.gold=std::max(0,gilda());menu.capacity=std::clamp(inventorySize(),1,100);menu.quantity=std::max(1,activeConsumable(menu.slot).quantity);menu.queued=false;}
    }else if(menu.page==Page::Graphics){
        const video::Shared shared=windowSettings();
        if(shared.open){
            if(pressed&kUp)menu.graphicsRow=(menu.graphicsRow+GraphicsRowEnd-1)%GraphicsRowEnd;
            else if(pressed&kDown)menu.graphicsRow=(menu.graphicsRow+1)%GraphicsRowEnd;
            if(menu.graphicsRow!=ApplyGraphics && (pressed&(kLeft|kRight)))stepGraphics(menu.graphicsRow,(pressed&kRight)?1:-1,shared);
            if(menu.graphicsRow==ApplyGraphics && (pressed&kCross)){
                // Borderless keeps the window size it returns to.
                video::Settings next=menu.graphics;
                if(next.mode==video::WindowMode::Borderless){next.width=shared.applied.width;next.height=shared.applied.height;}
                if(const auto post=videoPost.load(std::memory_order_acquire))post(next,true);menu.graphics=next;menu.graphicsStatus="Applied and saved";
            }
        }
    }else if(menu.page==Page::Quit){
        if(pressed&(kUp|kDown|kLeft|kRight))menu.quitChoice^=1;
        if(pressed&kCross){if(menu.quitChoice)quitRequested.store(true,std::memory_order_relaxed);else menu.page=Page::Root;}
    }else{
        if(pressed&kUp)menu.row=(menu.row+rowCount-1)%rowCount;else if(pressed&kDown)menu.row=(menu.row+1)%rowCount;
        int delta=0;if(pressed&kLeft)--delta;if(pressed&kRight)++delta;
        if(pressed&kL1)delta-=10;if(pressed&kR1)delta+=10;if(pressed&kL2)delta-=100;if(pressed&kR2)delta+=100;
        if(delta)menu.queued=false;
        if(Cheat cheat;toggleRow(menu.row,cheat)){
            if(pressed&kCross)setEnabled(cheat,!enabled(cheat));else if(pressed&kLeft)setEnabled(cheat,false);else if(pressed&kRight)setEnabled(cheat,true);
        }else if(menu.row==4 && equippedWeaponAttack()>=0)menu.attack=std::clamp(menu.attack+delta,0,999);
        else if(menu.row==5 && equippedWeaponAttack()>=0 && (pressed&kCross)){requestWeaponAttack(menu.attack);menu.queued=true;}
        else if(menu.row==6 && delta)setAbsMultiplier(absMultiplier()+delta);
        else if(menu.row==7 && gilda()>=0){
            int d=0;if(pressed&kLeft)--d;if(pressed&kRight)++d;if(pressed&kL1)d-=100;if(pressed&kR1)d+=100;if(pressed&kL2)d-=1000;if(pressed&kR2)d+=1000;
            menu.gold=std::clamp(menu.gold+d,0,65535);
        }else if(menu.row==8 && gilda()>=0 && (pressed&kCross)){requestGilda(menu.gold);menu.queued=true;}
        else if(menu.row==9 && delta){menu.slot=(menu.slot+(delta>0?1:2))%3;menu.quantity=std::max(1,activeConsumable(menu.slot).quantity);}
        else if(menu.row==10 && activeConsumable(menu.slot).id>=0)menu.quantity=std::clamp(menu.quantity+delta,1,std::max(1,activeConsumable(menu.slot).maximum));
        else if(menu.row==11 && activeConsumable(menu.slot).id>=0 && (pressed&kCross)){requestConsumableQuantity(menu.slot,menu.quantity);menu.queued=true;}
        else if(menu.row==12 && delta)setMovementPercent(movementPercent()+(delta>0?5:-5));
        else if(menu.row==14 && inventorySize()>=0)menu.capacity=std::clamp(menu.capacity+delta,std::min(100,minimumInventorySize()),100);
        else if(menu.row==15 && inventorySize()>=0 && (pressed&kCross)){requestInventorySize(menu.capacity);menu.queued=true;}
        else if(menu.row==LevelUp && equippedWeaponAttack()>=0 && (pressed&kCross)){requestWeaponLevelUp();menu.queued=true;}
        else if(menu.row==SpawnItem && delta && spawnItemCount()){
            const int n=spawnItemCount(),step=std::clamp(delta,-10,10);menu.spawn=((menu.spawn+step)%n+n)%n;
        }
        else if(menu.row==SpawnCount && delta)menu.spawnCount=std::clamp(menu.spawnCount+(delta>0?1:-1),1,10);
        else if(menu.row==GiveItem && spawnItemCount() && (pressed&kCross)){requestSpawn(spawnItem(menu.spawn).id,menu.spawnCount);menu.queued=true;}
        else if(menu.row==FastSpeed && delta)setFastForwardPercent(fastForwardPercent()+(delta>0?50:-50));
        else if(menu.row==DamageRate && delta)setDamageMultiplier(damageMultiplier()+delta);

    }
    done(c);
}
void draw(uint8_t *r,R5900Context *c,Runtime *rt)
{
    if(!menu.active || !menu.font || menu.page==Page::Game){originalDraw(r,c,rt);return;}
    if(read<uint16_t>(r,kLoaded)){
        const int phase=read<int32_t>(r,kPhase),fade=read<int32_t>(r,kFade);
        const int alpha=std::clamp(phase==1?fade*7:phase==2?128-fade*7:128,0,128);
        drawPage(alpha);
        // DrawMenuOption owns this counter; retain its exact transition cadence
        // when our page replaces the original Options drawing.
        write<int32_t>(r,kFade,phase?fade+1:0);
    }
    done(c);
}
bool customPage(){return menu.active && menu.font && menu.page!=Page::Game;}
void help(uint8_t *r,R5900Context *c,Runtime *rt){
    if(customPage() && getRegU32(c,31)==0x1F6598u)done(c);else originalHelp(r,c,rt);
}
void message(uint8_t *r,R5900Context *c,Runtime *rt){
    if(customPage() && getRegU32(c,31)==0x1F6640u && getRegU32(c,4)==0x1DA2350u)done(c);else originalMessage(r,c,rt);
}
}
void installIngameMenu(Runtime &rt,bozo::port::Registry &registry,void (*prepare)())
{
    originalInit=rt.functions.generated(kInit);originalExit=rt.functions.generated(kExit);
    originalKey=rt.functions.generated(kKey);originalDraw=rt.functions.generated(kDraw);
    if(!originalInit || !originalExit || !originalKey || !originalDraw)throw std::runtime_error("in-game cheats: Options handlers unavailable");
    originalHelp=registry.find(kHelp);if(!originalHelp)originalHelp=rt.functions.generated(kHelp);
    originalMessage=registry.find(kMessage);if(!originalMessage)originalMessage=rt.functions.generated(kMessage);
    if(originalHelp)registry.registerFunction(kHelp,help);
    if(originalMessage)registry.registerFunction(kMessage,message);
    menu={};prepareScene=prepare;quitRequested.store(false,std::memory_order_relaxed);
    registry.registerFunction(kInit,init);registry.registerFunction(kExit,exit);registry.registerFunction(kKey,key);registry.registerFunction(kDraw,draw);
}
bool takeIngameQuitRequest(){return quitRequested.exchange(false,std::memory_order_relaxed);}
void setGraphicsAccess(bozo::app::video::Shared (*shared)(),void (*post)(const bozo::app::video::Settings &,bool))
{videoPost.store(post,std::memory_order_release);videoShared.store(shared,std::memory_order_release);}
}
