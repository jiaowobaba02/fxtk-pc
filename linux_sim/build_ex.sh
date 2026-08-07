#!/bin/bash
# 用法: ./build_ex.sh ex01_hello
EX=${1:-ex01_hello}
EXDIR=examples
[ -d "$EXDIR" ] || EXDIR=../examples
echo "🔨 Building example: $EX (from $EXDIR)"
gcc -O2 -g -I. -I../components/fxtk \
    ../components/fxtk/fxtk.c ../components/fxtk/fxtk_draw.c \
    ../components/fxtk/fxtk_widgets.c ../components/fxtk/fxtk_font.c \
    ../components/fxtk/fxtk_effects.c \
    fxtk_sdl_driver.c fxtk_image_sdl.c \
    main_linux.c \
    $EXDIR/$EX.c -o $EXDIR/$EX \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lm -pthread -lX11
[ $? -eq 0 ] && $EXDIR/$EX || echo "❌ build failed"
