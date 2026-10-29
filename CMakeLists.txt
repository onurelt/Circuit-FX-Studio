cmake_minimum_required(VERSION 3.13)
project(CircuitFX C)
add_executable(CircuitFX psp/main.c src/render.c src/wav.c)
target_include_directories(CircuitFX PRIVATE src)
target_compile_options(CircuitFX PRIVATE -O2 -Wall -Wextra)
# Resolve runtime and debug dependencies before emitting firmware import stubs.
target_link_libraries(CircuitFX PRIVATE "-Wl,--start-group" m c pspdebug pspsdk cglue pthreadglue "-Wl,--end-group" pspdisplay pspge pspctrl pspaudio psppower pspuser)
create_pbp_file(TARGET CircuitFX TITLE "Circuit FX Studio Beta 1" VERSION 00.60 BUILD_PRX)
