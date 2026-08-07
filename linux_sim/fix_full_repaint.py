import re
from pathlib import Path

def fix(p: Path):
    if not p.exists():
        print("⏭️  不存在:", p); return
    s = p.read_text(encoding="utf-8")
    n = 0
    if "s_full" not in s:
        s, k = re.subn(r"static int s_repaint = 1;",
                       "static int s_repaint = 1;\nstatic int s_full = 0;", s, 1)
        n += k
        s, k = re.subn(r"void fx_repaint\(void\)\s*\{\s*s_repaint = 1;\s*s_dirty_n = 0;\s*\}",
                       "void fx_repaint(void) { s_full = 1; s_repaint = 1; s_dirty_n = 0; }", s, 1)
        n += k
        s, k = re.subn(r"if \(s_repaint\s*&\s*&\s*s_dirty_n\s*>\s*0\) \{",
                       "if (s_full) { s_full = 0; s_repaint = 0; s_dirty_n = 0;\n"
                       "fx_frame_begin(); fxtk_draw_all(); fx_frame_end(); }\n"
                       "else if (s_repaint && s_dirty_n > 0) {", s, 1)
        n += k
        if n: p.write_text(s, encoding="utf-8")
    print(f"✅ {p.name}: {n} 处" if n else f"⏭️  {p.name}: 已是修复版")

fix(Path("../components/fxtk/fxtk.c"))          # 开发目录
fix(Path("/media/user/Data/fxtk-v1.0/components/fxtk/fxtk.c"))  # 正式版目录
