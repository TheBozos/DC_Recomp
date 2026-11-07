// Complete compiled ReadInfo/wrappers and parser with synthetic pack/command
// metadata. IMG uses the real allocator and native memcpy; no model/texture boot.
#include "dc_functions.h"
#include "dun_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace
{
constexpr uint32_t gp=0x2a97f0,stack=0x80000,pack=0x400000,cfg=pack+0x200,
    image=pack+0x1800,arena=0x1f06660,destination=0x500000,table=0x1dc2e70,
    commands=0x251470,alternate=0x310000,input=0x600000;
int cases=0,failures=0,copies=0,alternateCalls=0;bozo::libs::Libraries libraries;
struct Rejected {};struct CallerEnd {};
void check(bool ok,const char *s) { if(!ok) {++failures;std::printf("FAIL ReadInfo case %d: %s\n",cases,s);} }
void word(uint8_t *p,uint32_t a,uint32_t n) {std::memcpy(p+a,&n,4);}
uint32_t word(const uint8_t *p,uint32_t a) {uint32_t n;std::memcpy(&n,p+a,4);return n;}
void boundary(uint8_t *ram,R5900Context *c,Runtime *rt)
{
    if(c->pc==0x139820 || c->pc==0x139850)
    {
        const auto entry=c->pc,link=c->r[31].uw[0];
        check(c->r[5].uw[0]==pack,"actual DUN call passes retained pack register");
        rt->functions.generated(entry)(ram,c,rt);
        check(c->pc==link && !c->leaving,"complete wrapper returns to actual DUN call continuation");
        throw CallerEnd{}; // subsequent character calls are outside this fixture
    }
    if(c->pc==alternate)
    {
        ++alternateCalls;check(c->r[31].uw[0]==0x139f3c,"ReadInfo reloads mutated command callback at real JALR");
        bozo::libs::Guest g(*rt,c);check(g.read32(g.read32(g.arg(0)))==1,"alternate callback receives parsed integer");
        c->pc=c->r[31].uw[0];return;
    }
    if(c->pc==0x104698 && c->r[31].uw[0]==0x1278d4) throw Rejected{};
    const char *name=c->pc==0x1033b0?"memcpy":c->pc==0x105568?"strcpy":c->pc==0x104698?"printf":nullptr;
    if(!name) throw std::runtime_error("unexpected ReadInfo library boundary");
    if(c->pc==0x1033b0) ++copies;
    bozo::libs::Guest g(*rt,c);libraries.find(name)->handler(libraries,g);c->pc=c->r[31].uw[0];
}
struct Host:bozo::ee::Host
{
    void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected library");}
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected VU0");}
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override {throw std::runtime_error("unexpected I/O read");}
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override {throw std::runtime_error("unexpected I/O write");}
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override {throw std::runtime_error(s);}
};
}
int main(int argc,char **argv)
{
    Host host;bozo::ee::Dispatcher dispatcher(host);auto &rt=dispatcher.runtime();auto *ram=rt.memory.ram();
    rt.functions.setGenerated(dc_function_table,dc_function_count);rt.functions.setResume(dc_resume_table,dc_resume_count);
    rt.functions.addModule(0x1dabd00,0x1f06b6c,ram+0x1dabd00,{0xaa},dun_function_table,dun_function_count,dun_resume_table,dun_resume_count);
    for(auto pc:{0x1033b0u,0x105568u,0x104698u,alternate})rt.functions.setOverride(pc,boundary);
    if(argc>1)
    {
        // Optional local leaf-only qualification; never reads assets in CI or
        // writes payload bytes to disk/output. Args come from pack inventory.
        if(argc<8 || std::string(argv[1])!="--preprocess-pack")return 2;
        const uint64_t offset=std::stoull(argv[3]);
        const uint32_t length=std::stoul(argv[4]),relative=std::stoul(argv[5]),size=std::stoul(argv[6]);
        if(length>0x1000000 || uint64_t(relative)+size>length)return 2;
        std::ifstream file(argv[2],std::ios::binary);file.seekg(offset);
        std::memset(ram,0,bozo::kFlatScratchpadOffset);file.read(reinterpret_cast<char *>(ram+pack),length);
        if(!file || uint64_t(file.gcount())!=length)return 2;
        constexpr uint32_t metadata=0x10000;
        word(ram,metadata,pack+relative);word(ram,metadata+4,size);
        const std::vector<uint8_t> before(ram,ram+bozo::kFlatScratchpadOffset);
        R5900Context c;c.r[4].ud[0]=metadata;c.r[31].ud[0]=0x123450;
        rt.functions.generated(0x13b270)(ram,&c,&rt);++cases;
        check(c.pc==0x123450 && !c.leaving,"complete artifact leaf returns");
        uint32_t changed=0,outside=0;
        for(size_t i=0;i<before.size();++i)if(ram[i]!=before[i])
        {
            ++changed;if(i<pack+relative || i>=pack+relative+size)++outside;
            check(ram[i]==32,"observed artifact byte changes are spaces");
        }
        check(outside==0,"artifact leaf preserves every RAM byte outside declared CFG");
        check(changed==std::stoul(argv[7]),"compiled changed-byte count matches static artifact walk");
        for(int i=8;i<argc;++i)
        {
            const uint32_t header=std::stoul(argv[i]);if(uint64_t(header)+80>length)return 2;
            check(!std::memcmp(ram+pack+header,before.data()+pack+header,80),"compiled artifact leaf preserves complete outer header");
        }
        std::printf("Artifact PreProcess leaf: changed=%u outside=%u headers=%d failures=%d\n",changed,outside,argc-8,failures);
        return failures?1:0;
    }
    auto setup=[&](const std::string &text,uint32_t used=0,uint32_t cap=100,uint32_t data=destination,uint32_t bytes=4)
    {
        copies=alternateCalls=0;std::memset(ram,0,bozo::kFlatScratchpadOffset);
        for(unsigned c='A';c<='Z';++c)ram[0x2967b1+c]=1;
        for(unsigned c='0';c<='9';++c)ram[0x2967b1+c]=4;
        // The compiled integer conversion uses the SDK division helper's
        // byte bit-length table. Initialize its mathematical schema rather
        // than copying retail data into this synthetic fixture.
        for(unsigned n=1;n<256;++n)
            for(unsigned v=n;v;v>>=1) {++ram[0x297770+n];++ram[0x297670+n];}
        // Synthetic descriptors have the same tested schema; other slots are
        // inert names. No retail descriptor bytes are part of this fixture.
        for(unsigned i=0;i<19;++i)
        {
            const auto row=0x250f60+i*68,name=0x620000+i*32;
            std::strcpy(reinterpret_cast<char *>(ram+name),i==0?"VERTEX_ANIME":i==15?"IMG":"UNUSED");
            word(ram,row,name);for(unsigned j=0;j<16;++j)word(ram,row+4+j*4,0xffffffffu);
            word(ram,commands+i*4,i==0?0x139f70:0x13aa50);
        }
        word(ram,0x250f60+4,1);word(ram,0x250f60+15*68+4,1);word(ram,0x250f60+15*68+8,0);
        std::strcpy(reinterpret_cast<char *>(ram+pack),"base.cfg");
        word(ram,pack+0x40,0x200);word(ram,pack+0x44,uint32_t(text.size()));word(ram,pack+0x48,0x1000);
        std::memcpy(ram+cfg,text.data(),text.size());
        std::strcpy(reinterpret_cast<char *>(ram+pack+0x1000),"image");
        word(ram,pack+0x1040,0x800);word(ram,pack+0x1044,bytes);word(ram,pack+0x1048,0x1000);
        for(unsigned i=0;i<64;++i)ram[image+i]=uint8_t(i*7+17);
        std::strcpy(reinterpret_cast<char *>(ram+0x610000),"base.cfg");
        word(ram,arena,data);word(ram,arena+8,used);word(ram,arena+12,cap);word(ram,table,0x12345678);
    };
    auto invoke=[&](uint32_t entry,uint32_t view=0)
    {
        R5900Context c;c.r[4].ud[0]=0x480000;c.r[5].ud[0]=pack|view;c.r[6].ud[0]=0x610000;c.r[7].ud[0]=arena;
        c.r[8].ud[0]=entry==0x139850?17:arena;c.r[9].ud[0]=entry==0x139850?arena:0;c.r[10].ud[0]=0;
        c.r[28].ud[0]=gp;c.r[29].ud[0]=stack;c.r[31].ud[0]=0x123450;
        rt.functions.generated(entry)(ram,&c,&rt);
        check(c.pc==0x123450 && !c.leaving && c.r[29].uw[0]==stack,"complete wrapper/ReadInfo returns with original stack");
        check(word(ram,gp-0x7374)==arena && word(ram,gp-0x7370)==arena && word(ram,gp-0x736c)==(entry==0x139850?arena:0u) && word(ram,gp-0x7368)==(entry==0x139850?arena:0u),"wrapper publishes actual DUN call arena roles, including optional IMG arena");
    };
    for(uint32_t entry:{0x139850u}) for(uint32_t view:{0u,0x20000000u,0x30000000u,0x80000000u,0xa0000000u})
    {
        ++cases;setup("// comment\nVERTEX_ANIME 1\nIMG 0,\"image\"\nIMG 1,\"image\"\n",3);
        const std::vector<uint8_t> header(ram+pack,ram+pack+80),next(ram+pack+0x1000,ram+pack+0x1050);
        invoke(entry,view);check(copies==2 && word(ram,arena+8)==5,"IMG advances existing shared cursor twice without reset");
        check(!std::memcmp(ram+destination+3*16,ram+image,4) && !std::memcmp(ram+destination+4*16,ram+image,4),"real IMG copies into successive reservations");
        check(!std::memcmp(ram+pack,header.data(),80) && !std::memcmp(ram+pack+0x1000,next.data(),80),"disjoint bounded normal fixture preserves both outer headers");
        check(ram[cfg]==' ' && word(ram,table)==0x12345678,"comments change source CFG but not DUN table");
        invoke(entry,view);check(copies==4 && word(ram,arena+8)==7,"second ReadInfo retains arena cursor across global parser-state reset");
    }
    for(uint32_t bytes:{0u,1u,15u,16u,17u,31u,32u,63u})
    {
        ++cases;setup("IMG 0,\"image\"\n",3,100,destination,bytes);invoke(0x139850);
        check(copies==1 && word(ram,arena+8)==3+(bytes>>4)+1,"IMG reserves next qword including zero/exact multiples before copy");
        check(!std::memcmp(ram+destination+48,ram+image,bytes),"IMG copies exact raw byte length");
        check(ram[destination+48+bytes]==0,"IMG leaves first byte after destination extent unchanged");
    }
    for(const char *text:{"IMG 0,\"missing\"\n","IMG -1,\"image\"\n","IMG 4,\"image\"\n"})
    {
        ++cases;setup(text,3);invoke(0x139850);
        check(copies==0 && word(ram,arena+8)==3,"missing member or invalid IMG slot does not allocate or copy");
    }
    {
        ++cases;setup("/*x*",3);
        // The declared CFG ends exactly at the following header. The lookup
        // already succeeded before PreProcess changes that header's first byte.
        const uint32_t next=cfg+4;
        std::strcpy(reinterpret_cast<char *>(ram+next),"/next");word(ram,pack+0x48,next-pack);
        word(ram,next+0x40,0x80);word(ram,next+0x44,4);word(ram,next+0x48,0x100);
        std::vector<uint8_t> expected(ram+pack,ram+pack+0x2000);
        std::memset(expected.data()+0x200,32,5);invoke(0x139850);
        check(!std::memcmp(ram+pack,expected.data(),expected.size()),"complete ReadInfo changes exactly CFG comments plus next header byte");
        check(copies==0 && word(ram,arena+8)==3,"header mutation needs no callbacks or cursor change");
    }
    for(uint32_t entry:{0x139850u})
    {
        ++cases;setup("IMG 0,\"image\"\n",0,100,table);invoke(entry);
        check(word(ram,table)==word(ram,image),"forged arena base lets actual IMG overwrite DUN dispatch bytes");
        ++cases;setup("IMG 0,\"image\"\n",0,100,pack);invoke(entry);
        check(word(ram,pack)==word(ram,image),"aliased arena lets actual IMG replace outer header bytes");
        ++cases;setup("IMG 0,\"image\"\n",0xffffffffu);invoke(entry);
        check(word(ram,arena+8)==0 && word(ram,destination-16)==word(ram,image),"negative shared cursor passes signed allocator check");
        ++cases;setup("IMG 0,\"image\"\n",100,100);bool rejected=false;
        try {invoke(entry);}catch(const Rejected &){rejected=true;}
        check(rejected && copies==0 && word(ram,table)==0x12345678,"IMG capacity rejection precedes payload copy");
        ++cases;setup("IMG 0,\"image\"\nVERTEX_ANIME 1\n",0,100,commands);word(ram,image,alternate);invoke(entry);
        check(alternateCalls==1 && word(ram,commands)==alternate,"bulk-alias mutation controls next command through reloaded table");
    }
    for(uint32_t view:{0u,0x20000000u,0x80000000u})
    {
        ++cases;setup("VERTEX_ANIME 1\nIMG 0,\"image\"\n",3);invoke(0x139820,view);
        check(copies==0 && word(ram,arena+8)==3,"LoadPackData without IMG arena skips IMG allocation/copy and preserves cursor");
        check(word(ram,gp-0x735c)==1,"non-IMG callback still executes through LoadPackData");
    }
    for(uint32_t used:{0u,3u,9u})
    {
        ++cases;setup("",used);
        // Complete leaf callback with explicitly seeded parser/character state;
        // this does not stand in for MOTION/AnimeDataInit's nested loaders.
        const uint32_t character=0x480000,motion=0x490000,keys=destination+used*16;
        word(ram,gp-0x7360,character);word(ram,gp-0x7370,arena);
        word(ram,gp-0x7340,keys);word(ram,gp-0x734c,2);word(ram,character+0xc20,motion);
        R5900Context c;c.r[28].ud[0]=gp;c.r[29].ud[0]=stack;c.r[31].ud[0]=0x123450;
        rt.functions.generated(0x13a6e0)(ram,&c,&rt);
        check(c.pc==0x123450 && !c.leaving && c.r[29].uw[0]==stack,"complete MOTION_END callback returns");
        check(word(ram,arena+8)==used+4 && word(ram,motion+0x64)==keys,"non-IMG motion callback advances existing shared cursor and publishes keys");
        check(word(ram,keys+32)==0xffffffffu && word(ram,keys+36)==0xffffffffu && word(ram,keys+40)==0xbf800000u,"motion callback writes terminal key after two explicit keys");
    }
    struct PreCase {const char *data;uint32_t size;const char *expected;};
    for(const auto &p: {PreCase{"plain\n..",6,"plain\n.."},PreCase{"//x\n..",4,"   \n.."},
                       PreCase{"/*x*/\n..",6,"     \n.."},PreCase{"/*x*/!",4,"     !"},
                       PreCase{"/*xx..",4,"    .."},PreCase{"abc/..",4,"abc/.."},PreCase{"//xx..",4,"    .."}})
    {
        ++cases;std::memset(ram,0,bozo::kFlatScratchpadOffset);std::memcpy(ram+cfg,p.data,std::strlen(p.data));
        word(ram,input,cfg);word(ram,input+4,p.size);
        auto expected=std::vector<uint8_t>(ram,ram+bozo::kFlatScratchpadOffset);std::memcpy(expected.data()+cfg,p.expected,std::strlen(p.expected));
        R5900Context c;c.r[4].ud[0]=input;c.r[31].ud[0]=0x123450;rt.functions.generated(0x13b270)(ram,&c,&rt);
        check(c.pc==0x123450 && !std::memcmp(ram,expected.data(),expected.size()),"complete PreProcess exact RAM effect, including one-past payload store");
    }
    for(auto pc:{0x139820u,0x139850u})rt.functions.setOverride(pc,boundary);
    for(auto entry:{0x1dba518u,0x1dba578u,0x1dba5e8u})
    {
        ++cases;setup("IMG 0,\"image\"\n",3);ram[0x1dabd00]=0xaa;
        const bool base=entry==0x1dba518;
        std::strcpy(reinterpret_cast<char *>(ram+0x1dc3168),"base.cfg");
        std::strcpy(reinterpret_cast<char *>(ram+0x1dc3178),"hand_up.cfg");
        if(!base)std::strcpy(reinterpret_cast<char *>(ram+pack),"hand_up.cfg");
        R5900Context c;c.pc=entry;c.r[19].ud[0]=pack;
        c.r[28].ud[0]=gp;c.r[29].ud[0]=stack;
        bool ended=false;
        try {rt.functions.resumeEntry(entry)->resume(ram,&c,&rt);}catch(const CallerEnd &){ended=true;}
        check(ended,"actual DUN argument span stops immediately after complete ReadInfo wrapper");
        check(copies==(base?1:0) && word(ram,arena+8)==(base?4u:3u),"DUN base call shares IMG arena; hand-up calls omit it and retain cursor");
        check(word(ram,gp-0x7374)==arena && word(ram,gp-0x7370)==arena && word(ram,gp-0x736c)==(base?arena:0u) && word(ram,gp-0x7368)==(base?arena:0u),"actual DUN spans publish proven arena bindings");
    }
    std::printf("ReadInfo/parser/shared-cursor effects: %d cases; %d failures\n",cases,failures);return failures?1:0;
}
