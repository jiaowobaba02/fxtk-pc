#!/bin/bash
# Windows 原生编译 (MSYS2 MINGW64 终端里运行):
#   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 \
#           mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-SDL2_image \
#           mingw-w64-x86_64-python
# 用法: ./build_win.sh            # 完整演示 (GPU 页自动降级 CPU)
#       ./build_win.sh ex01_hello # 独立示例
EX=${1:-app}
echo "🔨 Windows build: $EX"
python3 tools/gen_gpu_stub.py gpu_raymarch.h > gpu_stub_win.c
gcc -O2 -I. -I../components/fxtk \
    ../components/fxtk/fxtk.c ../components/fxtk/fxtk_draw.c \
    ../components/fxtk/fxtk_widgets.c ../components/fxtk/fxtk_font.c \
    ../components/fxtk/fxtk_effects.c \
    fxtk_sdl_driver.c fxtk_image_sdl.c main_linux.c \
    $( if [ "$EX" = "app" ]; then echo "app.c app_desktop.c raymarch.c gpu_stub_win.c"; else echo "examples/$EX.c"; fi ) \
    -o fxtk_win.exe \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lm -lpthread
[ $? -eq 0 ] && ./fxtk_win.exe || echo "❌ build failed"
