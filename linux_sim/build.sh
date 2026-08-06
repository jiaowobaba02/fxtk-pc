#!/bin/bash
echo "🔨 Compiling fxtk PC App (CPU+GPU Raymarch)..."
gcc -O2 -g -pthread \
    -I. -I../components/fxtk \
    ../components/fxtk/fxtk.c \
    ../components/fxtk/fxtk_draw.c \
    ../components/fxtk/fxtk_widgets.c \
    ../components/fxtk/fxtk_font.c \
    ../components/fxtk/fxtk_effects.c \
    fxtk_sdl_driver.c \
    fxtk_image_sdl.c \
    raymarch.c \
    gpu_raymarch.c \
    app_desktop.c \
    main_linux.c \
    app.c \
    -o fxtk_sim \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lm -pthread -lEGL -lGLESv2 -lX11
if [ $? -eq 0 ]; then
    echo "✅ Build successful! Running app..."
    ./fxtk_sim
else
    echo "❌ Build failed."
fi
