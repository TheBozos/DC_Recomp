#include "bozo/app/frame_pacer.h"
#include "bozo/audio/wav.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/host/free_host.h"
#include "bozo/port.h"
#include "dc_functions.h"
#include "dun_functions.h"
#include "title_functions.h"
#ifdef DC_AUDIT_INDIRECT
#include "dc_indirect_trace.h"
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifdef DC_WITH_WINDOW
#include "bozo/app/app.h"
#include "bozo/app/audio_device.h"
#include "bozo/app/device_pad.h"
#include "bozo/app/window.h"
#endif
namespace dc
{
    void installAudio(bozo::libs::Libraries &);
    void install(Runtime &, bozo::port::Registry &);
} // namespace dc
int main(int argc, char **argv)
{
    bozo::host::FreeOptions options;
    options.framePeriodCycles = 294912000 / 60;
    bool window = false;
#ifdef DC_WITH_WINDOW
    window = true;
    bozo::app::setIdentity({"Dark Cloud", "DC_Recomp", "dc-recomp", "SCUS_971.11"});
    bozo::app::WindowOptions windowOptions;
    windowOptions.title = "Dark Cloud - BozoEngine";
    windowOptions.width = 960;
    windowOptions.height = 720;
#endif
    options.elf = DC_DEFAULT_ELF;
    options.iso = DC_DEFAULT_ISO;
    options.frames = window ? 0 : 120;
    options.printGameOutput = true;
    std::string wav, livePad;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc)
            options.frames = std::stoul(argv[++i]);
        else if (a == "--pad-script" && i + 1 < argc)
            options.padScript = argv[++i];
        else if (a == "--live-pad" && i + 1 < argc)
            livePad = argv[++i];
        else if (a == "--card-dir" && i + 1 < argc)
            options.cardDir = argv[++i];
        else if (a == "--out" && i + 1 < argc)
            options.out = argv[++i];
        else if (a == "--elf" && i + 1 < argc)
            options.elf = argv[++i];
        else if (a == "--iso" && i + 1 < argc)
            options.iso = argv[++i];
        else if (a == "--wav" && i + 1 < argc)
            wav = argv[++i];
        else if (a == "--headless")
            window = false;
        else if (a == "--profile-io")
            options.profileIo = true;
#ifdef DC_WITH_WINDOW
        else if (a == "--no-vsync")
            windowOptions.vsync = false;
        else if (a == "--fullscreen")
            windowOptions.fullscreen = true;
        else if (a == "--capture-dir" && i + 1 < argc)
            windowOptions.captureDir = argv[++i];
        else if (a == "--capture-frame" && i + 1 < argc)
            windowOptions.captureFrames.push_back(std::stoull(argv[++i]));
#endif
        else
        {
            std::fprintf(stderr, "Usage: dc [--frames N] [--headless] [--elf FILE] [--iso FILE]\n"
                                 "          [--card-dir DIR] [--pad-script FILE | --live-pad FILE] [--wav FILE]\n"
                                 "          [--out FILE] [--capture-dir DIR] [--capture-frame N] [--profile-io]\n"
                                 "          [--fullscreen] [--no-vsync]\n");
            return a == "--help" ? 0 : 2;
        }
    }
#ifdef DC_WITH_WINDOW
    if (window && options.cardDir.empty())
    {
        bozo::app::migrateFrom(std::filesystem::path(options.elf).parent_path());
        options.cardDir = bozo::app::userDirectory().string();
    }
#endif
    options.realTime = window;
    bozo::host::FreeHost host(options);
    bozo::ee::Dispatcher d(host);
    dc::installAudio(host.libraries());
    d.setFunctions(dc_function_table, dc_function_count, dc_resume_table, dc_resume_count);
    bozo::port::Registry registry;
    dc::install(d.runtime(), registry);
    std::string error;
    if (!host.setUp(d, dc_external_table, dc_external_count, &registry, error))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    auto module = [&](const char *name, auto *table, size_t count, auto *resume, size_t resumes)
    {
        const std::string path = std::string(DC_IMAGE_DIR) + "/" + name + ".identity";
        std::ifstream in(path, std::ios::binary);
        std::vector<uint8_t> id{std::istreambuf_iterator<char>(in), {}};
        d.runtime().functions.addModule(0x1dabd00, 0x1f00000, d.runtime().memory.ram() + 0x1dabd00, std::move(id),
                                        table, count, resume, resumes);
    };
    module("title", title_function_table, title_function_count, title_resume_table, title_resume_count);
    module("dun", dun_function_table, dun_function_count, dun_resume_table, dun_resume_count);
    bozo::app::FramePacer pacer;
    pacer.setPaced(window);
    host.onFrame = [&](uint64_t f, const R5900Context *ctx)
    {
        pacer.frameEnd();
        if (f % 60 == 0)
            std::printf("[dc] frame=%llu pc=%x cycles=%llu\n", (unsigned long long)f, ctx->pc,
                        (unsigned long long)d.now());
    };
    bozo::libs::LiveScriptInput liveInput(livePad);
    if (!livePad.empty())
        host.libraries().pad.input = &liveInput;
    bozo::audio::WavWriter recording;
    if (!wav.empty() && !recording.open(wav))
    {
        std::fprintf(stderr, "cannot open WAV %s\n", wav.c_str());
        return 2;
    }
#ifdef DC_WITH_WINDOW
    bozo::app::DevicePad devices;
    bozo::app::AudioDevice audio;
    if (window && options.padScript.empty() && livePad.empty())
        host.libraries().pad.input = &devices;
#endif
    host.onAudio = [&](uint64_t, const int16_t *pcm, size_t count)
    {
        if (recording.isOpen())
            recording.write(pcm, count);
#ifdef DC_WITH_WINDOW
        if (window)
            audio.push(pcm, count);
#endif
    };
    auto run = [&]()
    {
#ifdef DC_WITH_WINDOW
        if (window)
            audio.open();
#endif
        uint64_t frames = 0;
        try
        {
            frames = host.run();
        }
        catch (const std::exception &e)
        {
            std::fprintf(stderr, "[dc] native failure: %s\n", e.what());
            return 1;
        }
        std::printf("[dc] frames=%llu end=%s\n", (unsigned long long)frames, host.endReason().c_str());
        const auto &stats = host.stats();
        std::printf("[dc] compatibility: %llu register accesses, %llu unknown; %llu native VU0 calls\n",
                    (unsigned long long)stats.ioAccesses, (unsigned long long)stats.ioUnknown,
                    (unsigned long long)stats.vu0Calls);
        const auto &io = host.libraries().io;
        for (const auto &[site, counts] : stats.ioSites)
            std::printf("[dc:io] pc=%08x address=%08x reads=%llu writes=%llu %s\n", site.first, site.second,
                        (unsigned long long)counts[0], (unsigned long long)counts[1], host.functionAt(site.first).c_str());
        std::printf("[dc] native transfer services: %llu VIF0, %llu VIF1, %llu GIF submissions; %llu sync calls\n",
                    (unsigned long long)io.nativeSubmissions[0], (unsigned long long)io.nativeSubmissions[1],
                    (unsigned long long)io.nativeSubmissions[2], (unsigned long long)io.nativeSynchronizations);
        return host.endReason().rfind("FAULT:", 0) == 0 ? 1 : 0;
    };
    int result;
#ifdef DC_WITH_WINDOW
    if (window)
    {
        bozo::app::Game game;
        game.run = run;
        game.stop = [&] { host.requestStop(); };
        game.framesEnded = [&] { return host.framesEnded(); };
        result = bozo::app::runWindow(windowOptions, game, &devices);
    }
    else
#endif
        result = run();
    recording.close();
#ifdef DC_AUDIT_INDIRECT
    if (!dcFinishIndirectTrace(result == 0)) result = 1;
#endif
    return result;
}
