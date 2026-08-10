#!/usr/bin/env python3
"""Post-process a cxx_migrate_hooks.py output for Amiga g++ quirks."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def camel(name: str) -> str:
    return name[0].lower() + name[1:]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--as-fn", required=True)
    ap.add_argument("--driver-class", required=True)
    args = ap.parse_args()

    path = Path(args.path)
    text = path.read_text()
    as_fn = args.as_fn

    # C++ keyword: Template *template → tmpl
    text = re.sub(
        r"(struct Template \*)template(\s*[,\)])",
        r"\1tmpl\2",
        text,
    )
    text = text.replace("template->", "tmpl->")
    text = re.sub(r",\s*template,", ", tmpl,", text)
    text = re.sub(r"\(bi, ri, template,", "(bi, ri, tmpl,", text)
    text = re.sub(r"blitTemplate\(ri, template,", "blitTemplate(ri, tmpl,", text)

    # variable named class
    text = re.sub(r"\bULONG class\b", "ULONG chipClass", text)
    text = re.sub(r", class\)", ", chipClass)", text)
    text = re.sub(r", class,", ", chipClass,", text)
    text = re.sub(r"\bclass\b(?=\s*;)", "chipClass", text)  # rare

    # LibName linkage
    for sym in ("LibName", "LibIdString", "LibVersion", "LibRevision"):
        text = re.sub(rf"(?<!extern )\bconst char {sym}\b", f"extern const char {sym}", text)
        text = re.sub(rf"(?<!extern )\bconst UWORD {sym}\b", f"extern const UWORD {sym}", text)
    text = text.replace("extern extern ", "extern ")

    # string literal to char* for ln_Name
    text = re.sub(
        r'(\.ln_Name\s*=\s*)"([^"]+)"',
        r'\1(char *)"\2"',
        text,
    )
    text = re.sub(
        r'(boardNode\.ln_Name\s*=\s*)"([^"]+)"',
        r'\1(char *)"\2"',
        text,
    )

    # Method bodies calling trampolines by PascalCase name
    tramp_start = text.find("/* P96 BoardInfo entry stubs */")
    if tramp_start < 0:
        tramp_start = text.find(f"static void ASM WaitBlitter")
    if tramp_start < 0:
        tramp_start = len(text)
    pre, post = text[:tramp_start], text[tramp_start:]

    # Common trampoline → method rewrites inside method section
    for hook in (
        "SetSpriteColor",
        "SetSpritePosition",
        "SetSpriteImage",
        "SetSprite",
        "FillRect",
        "WaitBlitter",
        "BlitRect",
        "InvertRect",
        "BlitTemplate",
        "BlitPattern",
        "DrawLine",
        "SetGC",
        "SetClock",
        "SetDAC",
        "SetColorArray",
        "SetDisplay",
        "SetPanning",
    ):
        method = camel(hook)
        pre, n = re.subn(rf"\b{hook}\s*\(\s*bi\s*,", f"this->{method}(", pre)
        pre, n2 = re.subn(rf"\b{hook}\s*\(\s*bi\s*\)", f"this->{method}()", pre)
        if n or n2:
            print(f"rewrite {hook}: {n + n2}")

    # Fix this-> in non-member helpers: convert to as_fn(bi)-> for waitBlitter only when
    # we can detect static functions - simpler: always use as_fn(bi)->waitBlitter()
    pre = pre.replace("this->waitBlitter()", f"{as_fn}(bi)->waitBlitter()")
    # Methods that still want this-> for fillRect etc keep this-> — OK with bi=this

    # waitFifo(bi, n) shadows method — route via as_fn
    pre, n = re.subn(r"\bwaitFifo\s*\(\s*bi\s*,\s*", f"{as_fn}(bi)->waitFifo(", pre)
    print(f"waitFifo via {as_fn}: {n}")

    # setWriteMask(bi,...) helper vs method — rename helper calls if helper exists
    if "static INLINE void setWriteMask(BoardInfo" in pre or "static INLINE void setWriteMask(BoardInfo_t" in pre:
        pre = re.sub(
            r"static INLINE void setWriteMask\(BoardInfo(_t)? \*bi,",
            r"static INLINE void setGEWriteMask(BoardInfo\1 *bi,",
            pre,
        )
        pre, n = re.subn(r"\bsetWriteMask\s*\(\s*bi\s*,", "setGEWriteMask(bi,", pre)
        print(f"setGEWriteMask: {n}")

    text = pre + post

    # interrupt DEFINE rename leftover
    text = text.replace(
        "DEFINE_INTSERVER(interruptServerTrampoline, VBlankInterruptHandler)",
        "DEFINE_INTSERVER(interruptServerTrampoline, interruptServer)",
    )
    text = text.replace(
        "static ULONG ASM interruptServer(",
        "ULONG ASM interruptServer(",
    )

    # Wrap file in extern C if missing
    if 'extern "C"' not in text[:800]:
        text = text.replace(
            '#include "common.h"\n',
            '#include "common.h"\n\n#ifdef __cplusplus\nextern "C" {\n#endif\n',
            1,
        )
        if not text.rstrip().endswith("}"):
            text = text.rstrip() + '\n\n#ifdef __cplusplus\n}\n#endif\n'

    # P96_HOOK only for PascalCase function symbols (not MaxBMWidth = 2048 / enums)
    def p96(m: re.Match) -> str:
        rhs = m.group(3)
        if not re.fullmatch(r"[A-Z][A-Za-z0-9]*", rhs):
            return m.group(0)
        if rhs.isupper():  # constants like TRUE
            return m.group(0)
        return f"{m.group(1)}P96_HOOK(bi->{m.group(2)}, {rhs});"

    text2, n = re.subn(r"^(\s*)bi->([A-Za-z0-9_]+)\s*=\s*([A-Za-z0-9_]+)\s*;\s*$", p96, text, flags=re.M)
    print(f"P96_HOOK: {n}")
    text = text2

    path.write_text(text)
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
