#!/usr/bin/env python3
"""从 gpu_raymarch.h 生成 Windows 空实现 (GPU 通道仅 Linux 可用)"""
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
src = re.sub(r'//.*', '', src)
src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
src = re.sub(r'(?m)^\s*#.*$', '', src)
out = ["/* auto-generated Windows stub */", "#include <stdint.h>", "#include <stddef.h>"]
for m in re.finditer(r'((?:[A-Za-z_]\w*(?:\s| \*|\*)+))([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*;', src):
    ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
    if ret in ("if", "for", "while", "return", "switch") or name in ("main",): continue
    body = "" if ret == "void" else ("return NULL;" if "*" in ret else "return 0;")
    out.append(f"{ret} {name}({args}) {{ {body} }}")
print("\n".join(out))
