#!/bin/bash
# 用法: ./build_ex.sh ex01_hello
EX=${1:-ex01_hello}
echo "🔨 Building example: $EX"
gcc -O2 -g -I. -I../components/fxtk \
    ../components/fxtk/fxtk.c ../components/fxtk/fxtk_draw.c \
    ../components/fxtk/fxtk_widgets.c ../components/fxtk/fxtk_font.c \
    ../components/fxtk/fxtk_effects.c \
    fxtk_sdl_driver.c fxtk_image_sdl.c \
    examples/$EX.c -o examples/$EX \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lm -pthread -lX11
[ $? -eq 0 ] && ./examples/$EX || echo "❌ build failed"
