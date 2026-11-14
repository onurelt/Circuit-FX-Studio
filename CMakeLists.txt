cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
endif()
project(CircuitFX C)
include("${VITASDK}/share/vita.cmake" REQUIRED)
add_executable(CircuitFX vita/main.c vita/ui.c src/render.c src/wav.c src/presets.c src/mp3_import.c)
target_include_directories(CircuitFX PRIVATE src vita third_party/minimp3)
target_compile_options(CircuitFX PRIVATE -O2 -std=gnu11 -Wall -Wextra)
target_link_libraries(CircuitFX m SceDisplay_stub SceCtrl_stub SceTouch_stub SceAudio_stub ScePower_stub SceIofilemgr_stub)
vita_create_self(CircuitFX.self CircuitFX UNSAFE)
file(GLOB SFX_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx/*")
set(SFX_ARGS)
foreach(SFX IN LISTS SFX_FILES)
  get_filename_component(SFX_NAME "${SFX}" NAME)
  list(APPEND SFX_ARGS FILE "${SFX}" "sfx/${SFX_NAME}")
endforeach()
vita_create_vpk(CircuitFX-Beta1.vpk CFXS00001 CircuitFX.self
  VERSION 01.06 NAME "Circuit FX Studio"
  ${SFX_ARGS}
  FILE assets/icon0.png sce_sys/icon0.png
  FILE assets/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/template.xml sce_sys/livearea/contents/template.xml)
