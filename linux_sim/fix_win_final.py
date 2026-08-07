import re
from pathlib import Path

print("🔧 终极修复 Windows 交叉编译...")

# 1. 彻底重写 build_win_cross.sh (解决 echo 报错)
sh_content = """#!/bin/bash
W=third_party/SDL2-win
S2=$W/SDL2-2.30.10/x86_64-w64-mingw32
TT=$W/SDL2_ttf-2.22.0/x86_64-w64-mingw32
IM=$W/SDL2_image-2.8.2/x86_64-w64-mingw32
INC="-I$S2/include -I$S2/include/SDL2 -I$TT/include -I$TT/include/SDL2 -I$IM/include -I$IM/include/SDL2"
LIB="-L$S2/lib -L$TT/lib -L$IM/lib"
echo "🔨 交叉编译 fxtk_win.exe ..."
# 确保 stub 存在
[ -f gpu_stub_win.c ] || python3 -c "
import re
src=open('gpu_raymarch.h').read()
src=re.sub(r'//.*','',src); src=re.sub(r'/\\*.*?\\*/','',src,flags=re.S); src=re.sub(r'(?m)^\\s*#.*$','',src)
out=['#include <stdint.h>','#include <stddef.h>']
for m in re.finditer(r'([A-Za-z_][\\w\\s\\*]*?)\\s+([A-Za-z_]\\w*)\\s*\\(([^;{]*)\\)\\s*;', src):
    ret,name,args=m.group(1).strip(),m.group(2),m.group(3).strip()
    if ret in ('if','for','while','return','switch','typedef','struct','extern'): continue
    body='' if ret=='void' else ('return NULL;' if '*' in ret else 'return 0;')
    out.append(f'{ret} {name}({args if args else \"void\"}) {{ {body} }}')
open('gpu_stub_win.c','w').write('\\n'.join(out))
"
x86_64-w64-mingw32-gcc -O2 -I. -I../components/fxtk $INC \\
    ../components/fxtk/fxtk.c ../components/fxtk/fxtk_draw.c \\
    ../components/fxtk/fxtk_widgets.c ../components/fxtk/fxtk_font.c \\
    ../components/fxtk/fxtk_effects.c \\
    fxtk_sdl_driver.c fxtk_image_sdl.c main_linux.c \\
    app.c app_desktop.c raymarch.c gpu_stub_win.c \\
    -o fxtk_win.exe $LIB \\
    -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lm -lpthread
if [ $? -eq 0 ]; then
    echo "📦 组装 dist/win ..."
    rm -rf dist/win && mkdir -p dist/win
    cp fxtk_win.exe dist/win/
    for d in "$S2/bin" "$TT/bin" "$IM/bin"; do cp -u $d/*.dll dist/win/ 2>/dev/null; done
    (cd dist && rm -f fxtk-v1.0-windows.zip && zip -qr fxtk-v1.0-windows.zip win)
    echo "🎉 完成: dist/fxtk-v1.0-windows.zip"
else
    echo "❌ 编译失败"
fi
"""
Path("build_win_cross.sh").write_text(sh_content, encoding="utf-8")
import os; os.chmod("build_win_cross.sh", 0o755)
print("✅ build_win_cross.sh: 已彻底重写")

# 2. main_linux.c: 注入 X11 Dummy 定义 (降维打击，无视具体代码结构)
p = Path("main_linux.c")
if p.exists():
    s = p.read_text(encoding="utf-8")
    if "X11_SKIP_DUMMY" not in s:
        dummy = """#ifdef _WIN32
#define X11_SKIP_DUMMY
typedef struct _Display Display;
typedef unsigned long Window;
typedef unsigned long Atom;
typedef unsigned long XID;
#define XOpenDisplay(name) ((Display*)0)
#define XCloseDisplay(display)
#define XInternAtom(display, name, only_if_exists) 0
#define XChangeProperty(display, w, property, type, format, mode, data, nelements)
#define XFlush(display)
#define XSync(display, discard)
#define DefaultScreen(display) 0
#define RootWindow(display, screen) 0
#endif

#ifndef _WIN32
#include <X11/Xlib.h>
#endif
"""
        # 把原有的 include 替换掉，并在最前面加上 dummy
        s = s.replace("#include <X11/Xlib.h>", "")
        s = dummy + s
        p.write_text(s, encoding="utf-8")
        print("✅ main_linux.c: 已注入 X11 Dummy 定义 (Windows 下自动空操作)")

print("\n🎉 终极修复完成！")
