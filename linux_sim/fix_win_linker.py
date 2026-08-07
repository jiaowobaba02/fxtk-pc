import re
from pathlib import Path

print("🔧 修复 Windows 链接错误...")

# 1. main_linux.c: 补充 XInitThreads dummy
p = Path("main_linux.c")
s = p.read_text(encoding="utf-8")
if "#define XInitThreads()" not in s:
    s = s.replace("#define XSync(display, discard)", "#define XSync(display, discard)\n#define XInitThreads()")
    p.write_text(s, encoding="utf-8")
    print("✅ main_linux.c: 已补充 XInitThreads dummy")

# 2. gpu_stub_win.c: 补充 gpu_raymarch_renderer 全局变量
p = Path("gpu_stub_win.c")
if p.exists():
    s = p.read_text(encoding="utf-8")
    if "gpu_raymarch_renderer" not in s:
        h = Path("gpu_raymarch.h").read_text(encoding="utf-8")
        m = re.search(r'extern\s+([A-Za-z_][\w\s\*]*?)\s+gpu_raymarch_renderer\s*;', h)
        if m:
            typ = m.group(1).strip()
            s += f"\n{typ} gpu_raymarch_renderer = {{0}};\n"
        else:
            s += "\nvoid *gpu_raymarch_renderer = 0;\n"
        p.write_text(s, encoding="utf-8")
        print("✅ gpu_stub_win.c: 已补充 gpu_raymarch_renderer 变量")

# 3. build_win_cross.sh: 修复 WinMain 错误 (添加 -lmingw32)
p = Path("build_win_cross.sh")
s = p.read_text(encoding="utf-8")
if "-lmingw32" not in s:
    s = s.replace("-lSDL2main -lSDL2", "-lmingw32 -lSDL2main -lSDL2")
    p.write_text(s, encoding="utf-8")
    print("✅ build_win_cross.sh: 已添加 -lmingw32 (解决 WinMain 错误)")

print("\n🎉 修复完成，重新编译！")
