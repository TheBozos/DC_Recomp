#include "bozo/app/frame_pacer.h"
#include "bozo/audio/wav.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/host/free_host.h"
#include "bozo/port.h"
#include "dc_functions.h"
#include "dc_cheats.h"
#include "dc_ingame_cheats.h"
#include "dc_pad.h"
#include "dc_state.h"
#include "dc_warp.h"
#include <string>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "dc_native_api.h"
#include "dun_functions.h"
#include "title_functions.h"
#ifdef DC_AUDIT_INDIRECT
#include "dc_indirect_trace.h"
#endif
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef DC_WITH_WINDOW
#include "bozo/app/app.h"
#include "bozo/app/audio_device.h"
#include "bozo/app/device_pad.h"
#include "bozo/app/video_settings.h"
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
    [[maybe_unused]] bool mute = false; // no sound device: --mute, and every --hidden run
    std::string cheatScript; // --cheat-script FILE
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
    std::string wav, livePad, assetDir, dumpRam;
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
        else if (a == "--asset-dir" && i + 1 < argc)
            assetDir = argv[++i];
        else if (a == "--iso" && i + 1 < argc)
            options.iso = argv[++i];
        else if (a == "--wav" && i + 1 < argc)
            wav = argv[++i];
        else if (a == "--headless")
            window = false;
        else if (a == "--profile-io")
            options.profileIo = true;
        else if (a == "--dump-ram" && i + 1 < argc)
            dumpRam = argv[++i];
        else if (a == "--load-state" && i + 1 < argc)
            options.loadState = argv[++i];
        else if (a == "--load-state-at" && i + 1 < argc)
            options.loadStateFrame = std::stoul(argv[++i]);
        else if (a == "--save-state" && i + 1 < argc)
            options.saveState = argv[++i];
        else if (a == "--save-state-at" && i + 1 < argc)
            options.saveStateFrame = std::stoul(argv[++i]);
#ifdef DC_WITH_WINDOW
        else if (a == "--no-vsync")
            windowOptions.vsync = false;
        else if (a == "--fullscreen")
            windowOptions.fullscreen = true;
        else if (a == "--hidden") // never shown, and silent; captures read the game's offscreen picture
            windowOptions.hidden = mute = true;
        else if (a == "--cheat-script" && i + 1 < argc) // travel/progress commands by frame (dc_warp.cpp)
            cheatScript = argv[++i];
        else if (a == "--mute") // no sound device (a --wav recording still gets every sample)
            mute = true;
        else if (a == "--capture-dir" && i + 1 < argc)
            windowOptions.captureDir = argv[++i];
        else if (a == "--capture-frame" && i + 1 < argc)
            windowOptions.captureFrames.push_back(std::stoull(argv[++i]));
#endif
        else
        {
            std::fprintf(stderr, "Usage: dc [--frames N] [--headless] [--elf FILE] [--iso FILE]\n"
                                 "          [--asset-dir DIR] [--card-dir DIR] [--pad-script FILE | --live-pad FILE] [--wav FILE]\n"
                                 "          [--out FILE] [--capture-dir DIR] [--capture-frame N] [--profile-io]\n"
                                 "          [--fullscreen] [--hidden] [--mute] [--no-vsync] [--dump-ram FILE]\n"
                                 "          [--load-state FILE [--load-state-at FRAME]] [--save-state FILE --save-state-at FRAME]\n"
                                 "          [--cheat-script FILE]\n");
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
    if (!cheatScript.empty() && !dc::warp::loadScript(cheatScript))
        return 2;
    if (assetDir.empty())
    {
        const auto besideElf = std::filesystem::absolute(options.elf).parent_path();
        if (std::filesystem::is_regular_file(besideElf / "dc_textures.bin")) assetDir = besideElf.string();
    }
    dc::native::setAssetDirectory(assetDir);
    options.realTime = window;
    options.gameTag = "SCUS-97111"; // a save state loads only into Dark Cloud
    bozo::host::FreeHost host(options);
    bozo::ee::Dispatcher d(host);
    dc::installAudio(host.libraries());
    d.setFunctions(dc_function_table, dc_function_count, dc_resume_table, dc_resume_count);
    bozo::port::Registry registry;
    dc::install(d.runtime(), registry);
    dc::cheats::install(d.runtime(), registry);
    dc::warp::install(d.runtime(), registry);
    dc::cheats::installIngameMenu(d.runtime(), registry, dc::native::finishSceneForMenu);
    dc::pad::install(registry);
    // Save states: DC's own part (the engine saves its runtime, libraries and host).
    host.stateSections()->add({"dc", [&](bozo::state::Archive &a) { dc::state::serialize(a, d.runtime()); },
                               [] { return dc::state::unsaveable(); }});
    std::string error;
    if (!host.setUp(d, dc_external_table, dc_external_count, &registry, error))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    auto module = [&](const char *name, auto *table, size_t count, auto *resume, size_t resumes)
    {
        const auto path = (assetDir.empty() ? std::filesystem::path(DC_IMAGE_DIR)
                                             : std::filesystem::path(assetDir) / "native_images") / (std::string(name) + ".identity");
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Missing native image identity: " + path.string());
        std::vector<uint8_t> id{std::istreambuf_iterator<char>(in), {}};
        d.runtime().functions.addModule(0x1dabd00, 0x1f00000, d.runtime().memory.ram() + 0x1dabd00, std::move(id),
                                        table, count, resume, resumes);
    };
    module("title", title_function_table, title_function_count, title_resume_table, title_resume_count);
    module("dun", dun_function_table, dun_function_count, dun_resume_table, dun_resume_count);
    bozo::app::FramePacer pacer;
    pacer.setPaced(window);
    const uint32_t normalCap = pacer.cap(); // 60, or PS2X_FRAME_CAP (0: unlimited, left alone)
    host.onFrame = [&](uint64_t f, const R5900Context *ctx)
    {
        dc::warp::onFrame(f);
        if (dc::cheats::takeIngameQuitRequest()) host.requestStop();
#ifdef DC_WITH_WINDOW
        // Fast-forward always on runs at the Video tab's fast-forward speed (the one the engine's hold uses).
        if (window && dc::cheats::enabled(dc::cheats::Cheat::FastForwardLock))
            if (const auto shared = bozo::app::video::shared(); shared.open)
                dc::cheats::setFastForwardPercent(shared.applied.fastForward);
        // Widescreen (the Video tab, video.ini's widescreen, PS2X_WIDESCREEN): a 16:9 picture showing more at the sides,
        // the HUD at the edges. A hidden run (a test) reads it from the environment alone, never the player's video.ini.
        if (window)
        {
            static const bool s_hiddenWide = [] { const char *e = std::getenv("PS2X_WIDESCREEN"); return e && (e[0] == '1' || e[0] == 'o'); }();
            bool wide = s_hiddenWide;
            if (!windowOptions.hidden)
                if (const auto shared = bozo::app::video::shared(); shared.open)
                    wide = shared.applied.widescreen;
            dc::native::setWidescreen(wide);
            bozo::app::setPictureAspect(wide ? 16.0f / 9.0f : 0.0f);
        }
#endif
        // Fast-forward always on raises the cap: the game's logic and clock advance per frame, so it simply runs more
        // of them. While the engine's fast forward is held (FramePacer::speedPercent), the pacer already runs the cap at
        // that speed: the cap stays normal then, so the two never stack.
        if (normalCap)
        {
            const bool engineFast = bozo::app::FramePacer::speedPercent().load(std::memory_order_relaxed) != 100u;
            pacer.setCap(engineFast ? normalCap : normalCap * static_cast<uint32_t>(dc::cheats::gameSpeedPercent()) / 100u);
        }
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
        // Muted while fast-forwarding: sound made faster than real time would overflow the device's queue and be
        // dropped in bursts. A WAV recording keeps every sample.
        if (window && !mute && dc::cheats::gameSpeedPercent() == 100 &&
            bozo::app::FramePacer::speedPercent().load(std::memory_order_relaxed) == 100u)
            audio.push(pcm, count);
#endif
    };
    auto run = [&]()
    {
#ifdef DC_WITH_WINDOW
        if (window && !mute)
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
        if (!dumpRam.empty())
            if (FILE *f = std::fopen(dumpRam.c_str(), "wb"))
            {
                std::fwrite(d.runtime().memory.ram(), 1, 0x2000000u, f);
                std::fclose(f);
            }
        std::printf("[dc] frames=%llu end=%s\n", (unsigned long long)frames, host.endReason().c_str());
        const auto &stats = host.stats();
        std::printf("[dc] compatibility: %llu register accesses, %llu unknown; %llu native VU0 calls\n",
                    (unsigned long long)stats.ioAccesses, (unsigned long long)stats.ioUnknown,
                    (unsigned long long)stats.vu0Calls);
        std::printf("[dc] calls: %llu kernel, %llu library, %llu port overrides\n", (unsigned long long)stats.kernelCalls,
                    (unsigned long long)stats.libraryCalls, (unsigned long long)stats.overrideCalls);
        // DC_CALL_STATS=1: the native library calls by function, most first.
        if (std::getenv("DC_CALL_STATS"))
        {
            std::vector<std::pair<uint64_t, std::string>> byName;
            for (const auto &[name, count] : stats.libraryByName)
                byName.emplace_back(count, name);
            std::sort(byName.rbegin(), byName.rend());
            for (const auto &[count, name] : byName)
                std::printf("[dc:lib] %10llu %s\n", (unsigned long long)count, name.c_str());
        }
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
        dc::cheats::registerMenu();
        // video.ini (the launcher's Settings, the F2 Video tab and the in-game Graphics page) over the defaults, and the
        // environment over the file; --fullscreen keeps its meaning, the monitor as it is.
        bozo::app::video::loadAtStartup(windowOptions.width, windowOptions.height);
        if (windowOptions.fullscreen)
            bozo::app::video::edited().mode = bozo::app::video::WindowMode::Borderless;
        windowOptions.videoSettings = true;
        bozo::app::video::offerWidescreen(true); // the Video tab's Widescreen (see the frame hook)
        dc::cheats::setGraphicsAccess(&bozo::app::video::shared, &bozo::app::video::post);
        bozo::app::Game game;
        game.run = run;
        game.stop = [&] { host.requestStop(); };
        game.framesEnded = [&] { return host.framesEnded(); };
        // F5 saves the selected slot, F7 loads it, F6 / Shift+F6 pick the slot (0-9); also F2's Save states tab.
        game.saveState = [&](int slot) { host.requestSave(bozo::app::stateSlotPath(slot)); };
        game.loadState = [&](int slot) { host.requestLoad(bozo::app::stateSlotPath(slot)); };
        host.onStateMessage = [](bool, const std::string &message) { bozo::app::notify(message); };
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
