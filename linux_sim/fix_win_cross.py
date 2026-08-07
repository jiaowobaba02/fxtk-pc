import re
from pathlib import Path

print("🔧 修复 Windows 交叉编译问题...")

# 1. raymarch.c: sysconf 跨平台修复 (Windows 用 GetSystemInfo)
p = Path("raymarch.c")
if p.exists():
    s = p.read_text(encoding="utf-8")
    if "_WIN32" not in s and "sysconf" in s:
        s = "#ifdef _WIN32\n#include <windows.h>\n#endif\n" + s
        s = s.replace("int n = (int)sysconf(_SC_NPROCESSORS_ONLN);",
                      "int n;\n#ifdef _WIN32\n    SYSTEM_INFO sysinfo; GetSystemInfo(&sysinfo); n = sysinfo.dwNumberOfProcessors;\n#else\n    n = (int)sysconf(_SC_NPROCESSORS_ONLN);\n#endif")
        p.write_text(s, encoding="utf-8")
        print("✅ raymarch.c: CPU 核心数获取已跨平台")

# 2. main_linux.c: X11 依赖隔离 (Windows 下自动跳过 X11 代码)
p = Path("main_linux.c")
if p.exists():
    s = p.read_text(encoding="utf-8")
    if "XOpenDisplay" in s and "#ifndef _WIN32" not in s:
        lines = s.split('\n')
        out = []
        for l in lines:
            # 识别 X11 相关代码并包裹 #ifndef _WIN32
            if any(kw in l for kw in ['X11/Xlib', 'XOpenDisplay', 'XCloseDisplay', 'XInternAtom', 
                                      'XChangeProperty', 'XFlush', 'XSync', 'Display *', 'Display*', 
                                      'Window x11', 'Atom ', 'XA_CARDINAL', 'PropModeReplace']):
                out.append('#ifndef _WIN32')
                out.append(l)
                out.append('#endif')
            else:
                out.append(l)
        p.write_text('\n'.join(out), encoding="utf-8")
        print("✅ main_linux.c: X11 依赖已隔离 (Windows 下自动跳过)")

# 3. 生成 GPU stub (直接在当前目录生成，不依赖 tools/)
if not Path("gpu_stub_win.c").exists() or Path("gpu_stub_win.c").stat().st_size < 10:
    p = Path("gpu_raymarch.h")
    if p.exists():
        src = p.read_text(encoding="utf-8")
        src = re.sub(r'//.*', '', src)
        src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
        src = re.sub(r'(?m)^\s*#.*$', '', src)
        out = ["/* auto-generated Windows stub */", "#include <stdint.h>", "#include <stddef.h>"]
        for m in re.finditer(r'([A-Za-z_][\w\s\*]*?)\s+([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*;', src):
            ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
            if ret in ("if", "for", "while", "return", "switch", "typedef", "struct", "extern") or name in ("main",): continue
            body = "" if ret == "void" else ("return NULL;" if "*" in ret else "return 0;")
            if not args.strip(): args = "void"
            out.append(f"{ret} {name}({args}) {{ {body} }}")
        Path("gpu_stub_win.c").write_text("\n".join(out), encoding="utf-8")
        print("✅ gpu_stub_win.c: 已直接在当前目录生成")

# 4. 更新 build_win_cross.sh (去掉 -lX11, 修复路径)
p = Path("build_win_cross.sh")
if p.exists():
    s = p.read_text(encoding="utf-8")
    s = s.replace("-lX11", "")
    s = s.replace("tools/gen_gpu_stub.py gpu_raymarch.h > gpu_stub_win.c", 
                  "echo 'using pre-generated gpu_stub_win.c'")
    p.write_text(s, encoding="utf-8")
    print("✅ build_win_cross.sh: 已移除 -lX11 并修复路径")

print("\n🎉 修复完成！")
