// Dark Cloud's sound-library selection. Engine services and translation are game independent.
#include "dc_audio_modules.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
namespace dc
{
    namespace
    {
        std::unique_ptr<bozo::native::irx::CompiledLibraries> sound;
    }
    void installAudio(bozo::libs::Libraries &libraries)
    {
        sound = std::make_unique<bozo::native::irx::CompiledLibraries>(libraries, 0x80000);
        addDcAudioImages(*sound);
        libraries.mc.slotMask = 3; // DC uses slot 1 for the physical port card.
        libraries.audioProvider = [](uint64_t cycle) { return sound->advanceTo(cycle); };
        libraries.sif.transfer = [](uint32_t address, const uint8_t *bytes, uint32_t count)
        { sound->writeBuffer(address, bytes, count); };
        libraries.sif.allocateBuffer = [](uint32_t bytes) { return sound->allocateBuffer(bytes); };
        libraries.sif.freeBuffer = [](uint32_t address) { return sound->freeBuffer(address); };
        libraries.sif.resetProvider = [] { sound->reset(); };
        libraries.sif.moduleLoader = [](const std::string &path)
        {
            auto name = path.substr(path.find_last_of("/\\") + 1);
            if (const auto version = name.find(';'); version != std::string::npos)
                name.resize(version);
            for (char &c : name)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            // These services are answered directly at their EE APIs by Bozo's native pad and memory-card libraries.
            if (name == "SIO2MAN.IRX" || name == "PADMAN.IRX" || name == "MCMAN.IRX" || name == "MCSERV.IRX")
                return 1;
            return sound->startImage(name + ";1");
        };
    }
} // namespace dc
