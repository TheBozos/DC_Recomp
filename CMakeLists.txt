cmake_minimum_required(VERSION 3.21)
project(DC_Recomp LANGUAGES CXX)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(DC_WINDOW "Build the SDL3 native game window" ON)
set(BOZO_APP ${DC_WINDOW} CACHE BOOL "" FORCE)
set(BOZO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BOZO_REQUIRE_FFMPEG ON CACHE BOOL "Native movie decoding is required by this port" FORCE)
set(DC_ISO "${CMAKE_SOURCE_DIR}/Dark Cloud (USA).iso" CACHE FILEPATH "User-owned NTSC-U disc image")
add_subdirectory(BozoEngine)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(dc_gen ${CMAKE_BINARY_DIR}/dc_gen)
file(MAKE_DIRECTORY ${dc_gen})
set(dc_images ${dc_gen}/dc.elf ${dc_gen}/title.elf ${dc_gen}/dun.elf
              ${dc_gen}/title.identity ${dc_gen}/dun.identity)
add_custom_command(OUTPUT ${dc_images}
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/dc_bozo_prepare.py ${CMAKE_SOURCE_DIR} ${dc_gen}
  DEPENDS tools/dc_bozo_prepare.py tools/wrap_elf.py tools/symtab_map.py
          game/SCUS_971.11 game/TITLE.BIN game/DUN.BIN recomp/maps/dc_title.csv recomp/maps/dc_dun.csv
  COMMENT "Preparing the resident and overlay ELF images offline")
set(dc_externals ${dc_gen}/dc_externals.txt)
add_custom_command(OUTPUT ${dc_externals}
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/dc_bozo_externals.py --overrides ${CMAKE_SOURCE_DIR} ${dc_gen}/dc_overrides.txt
  COMMAND bozo-externals ${CMAKE_SOURCE_DIR}/game/SCUS_971.11 ${dc_gen}/native_externals.txt --overrides ${dc_gen}/dc_overrides.txt
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/dc_bozo_externals.py ${dc_gen}/native_externals.txt ${dc_externals}
  DEPENDS bozo-externals ${CMAKE_SOURCE_DIR}/tools/dc_bozo_externals.py ${CMAKE_SOURCE_DIR}/runtime/dc_native.cpp
          ${CMAKE_SOURCE_DIR}/runtime/dc_port.cpp ${CMAKE_SOURCE_DIR}/game/SCUS_971.11)
set(dc_sources)
foreach(image dc title dun)
  set(chunks 32)
  if(NOT image STREQUAL "dc")
    set(chunks 4)
  endif()
  set(files ${dc_gen}/${image}_functions.h ${dc_gen}/${image}_table.cpp)
  math(EXPR last "${chunks} - 1")
  foreach(i RANGE ${last})
    string(LENGTH "${i}" len)
    if(len EQUAL 1)
      set(n "00${i}")
    else()
      set(n "0${i}")
    endif()
    list(APPEND files ${dc_gen}/${image}_${n}.cpp)
  endforeach()
  add_custom_command(OUTPUT ${files}
    COMMAND bozo-recomp ${dc_gen}/${image}.elf ${dc_gen} --prefix ${image} --symbol-prefix ${image}_ --files ${chunks} --no-comments --externals ${dc_externals}
    DEPENDS bozo-recomp ${dc_gen}/${image}.elf ${dc_externals})
  list(APPEND dc_sources ${files})
endforeach()
add_custom_target(dc_generate DEPENDS ${dc_sources} ${dc_images})
add_library(dc_game STATIC ${dc_sources})
target_link_libraries(dc_game PUBLIC bozo_runtime_headers)
target_include_directories(dc_game PUBLIC ${dc_gen})
target_compile_options(dc_game PRIVATE -O1 -w -ffp-contract=off)
add_executable(dc-headless runtime/dc_main.cpp runtime/dc_port.cpp runtime/dc_native.cpp)
target_link_libraries(dc-headless PRIVATE dc_game bozo_host)
if(MINGW)
  target_link_options(dc-headless PRIVATE -static)
endif()
target_compile_definitions(dc-headless PRIVATE DC_IMAGE_DIR="${dc_gen}")

set(dc_audio_gen ${CMAKE_BINARY_DIR}/dc_audio_gen)
set(dc_audio_outputs ${dc_audio_gen}/dc_audio_modules.h)
set(dc_audio_inputs)
foreach(module LIBSD SDRDRV MODMIDI MODHSYN EZMIDI)
  string(TOLOWER "${module}" lower)
  list(APPEND dc_audio_outputs ${dc_audio_gen}/${lower}_iop.cpp)
  list(APPEND dc_audio_inputs ${CMAKE_SOURCE_DIR}/game/MODULES/${module}.IRX)
endforeach()
add_custom_command(OUTPUT ${dc_audio_outputs}
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/dc_bozo_audio.py ${CMAKE_SOURCE_DIR} ${dc_audio_gen}
  DEPENDS ${dc_audio_inputs} tools/dc_bozo_audio.py
          BozoEngine/tools/native_irx/relocate.py BozoEngine/tools/native_irx/recompile.py
  COMMENT "Statically recompiling the native sound libraries (strict mode)")
add_custom_target(dc_native_audio DEPENDS ${dc_audio_outputs})
add_library(dc_audio_code OBJECT ${dc_audio_gen}/libsd_iop.cpp ${dc_audio_gen}/sdrdrv_iop.cpp ${dc_audio_gen}/modmidi_iop.cpp ${dc_audio_gen}/modhsyn_iop.cpp ${dc_audio_gen}/ezmidi_iop.cpp)
target_link_libraries(dc_audio_code PRIVATE bozo_runtime_headers)
target_compile_options(dc_audio_code PRIVATE -O1 -w)
target_sources(dc-headless PRIVATE $<TARGET_OBJECTS:dc_audio_code> runtime/dc_audio.cpp)
target_include_directories(dc-headless PRIVATE ${dc_audio_gen})
add_dependencies(dc-headless dc_native_audio)

if(DC_WINDOW)
  add_executable(dc runtime/dc_main.cpp runtime/dc_port.cpp runtime/dc_native.cpp runtime/dc_audio.cpp $<TARGET_OBJECTS:dc_audio_code>)
  target_link_libraries(dc PRIVATE dc_game bozo_host bozo_app)
  if(WIN32)
    enable_language(RC)
    target_sources(dc PRIVATE runtime/app/dc.rc)
    target_include_directories(dc PRIVATE runtime/app)
  endif()
  target_include_directories(dc PRIVATE ${dc_audio_gen})
  add_dependencies(dc dc_native_audio)
  target_compile_definitions(dc PRIVATE DC_WITH_WINDOW DC_IMAGE_DIR="${dc_gen}")
  if(MINGW)
    target_link_options(dc PRIVATE -static)
  endif()
endif()

# Offline texture conversion is the only place that knows the disc's console texture layout.
set(dc_textures ${CMAKE_BINARY_DIR}/textures/dc_textures.bin)
add_custom_command(OUTPUT ${dc_textures}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/textures
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/dc_texconv.py ${CMAKE_SOURCE_DIR}/game ${dc_textures}
  DEPENDS tools/dc_texconv.py tools/gs_layout.py game/DATA.HD2 game/DATA.DAT
  COMMENT "Converting the user-owned disc textures offline")
add_custom_target(dc_assets DEPENDS ${dc_textures})
foreach(target dc-headless dc)
  if(TARGET ${target})
    add_dependencies(${target} dc_assets)
    target_compile_definitions(${target} PRIVATE
      DC_DEFAULT_TEXTURES="${dc_textures}" DC_DEFAULT_ELF="${CMAKE_SOURCE_DIR}/game/SCUS_971.11"
      DC_DEFAULT_ISO="${DC_ISO}")
  endif()
endforeach()

option(DC_ENGINE_TESTS "Build BozoEngine's regression suites alongside the port" OFF)
if(DC_ENGINE_TESTS)
  enable_testing()
  set(BOZO_TEST_ELF "${dc_gen}/dc.elf")
  add_subdirectory(BozoEngine/tests engine_tests)
endif()
