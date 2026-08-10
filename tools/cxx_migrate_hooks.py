#!/usr/bin/env python3
"""
Mechanically convert P96 chip-driver ASM hook functions toward Mach64-style
C++ class methods plus thin C trampolines.

Reads a .c/.cpp file (or stdin), emits transformed source on stdout or -o.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence, Tuple

RETURN_TYPES = ("void", "BOOL", "ULONG", "UWORD", "LONG", "APTR")
RETURN_RE = "|".join(re.escape(t) for t in RETURN_TYPES)

BI_PARAM_RE = re.compile(
    r"__REG(?:A0|A1)\s*\(\s*struct\s+BoardInfo\s*\*\s*bi\s*\)",
    re.DOTALL,
)

HOOK_HEAD_RE = re.compile(
    rf"(?P<prefix>(?:(?:^|\n)(?P<indent>[ \t]*)(?P<static>static\s+)?"
    rf"(?P<ret>{RETURN_RE})\s+ASM\s+"
    rf"(?P<name>[A-Z][A-Za-z0-9_]*)\s*\())",
    re.MULTILINE,
)

FORWARD_HEAD_RE = re.compile(
    rf"(?:(?:^|\n)(?P<indent>[ \t]*)(?P<static>static\s+)?"
    rf"(?P<ret>{RETURN_RE})\s+ASM\s+"
    rf"(?P<name>[A-Z][A-Za-z0-9_]*)\s*\()",
    re.MULTILINE,
)

INITCHIP_RE = re.compile(
    r"\bBOOL\s+InitChip\s*\(\s*__REGA0\s*\(\s*struct\s+BoardInfo\s*\*\s*bi\s*\)\s*\)\s*\{",
    re.MULTILINE,
)

TRAMPOLINE_MARKER = "/* CXX_TRAMPOLINES */"

P96_ASSIGN_RE = re.compile(
    r"^(\s*)bi->([A-Za-z0-9_]+)\s*=\s*([A-Za-z0-9_]+)\s*;\s*$"
)


@dataclass
class HookFunction:
    start: int
    end: int
    indent: str
    is_static: bool
    ret_type: str
    name: str
    params_raw: str
    body: str
    bi_reg: str  # "A0" or "A1"


def find_matching_paren(text: str, open_pos: int) -> int:
    assert text[open_pos] == "("
    depth = 0
    i = open_pos
    while i < len(text):
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        elif ch in "\"'":
            quote = ch
            i += 1
            while i < len(text):
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    break
                i += 1
        i += 1
    raise ValueError(f"unbalanced '(' at {open_pos}")


def find_function_body(text: str, close_paren: int) -> Tuple[int, int]:
    i = close_paren + 1
    while i < len(text) and text[i].isspace():
        i += 1
    if i >= len(text) or text[i] != "{":
        raise ValueError("expected '{' after function parameter list")
    start = i
    depth = 0
    while i < len(text):
        ch = text[i]
        if ch == "/" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "/":
                i += 2
                while i < len(text) and text[i] != "\n":
                    i += 1
                continue
            if nxt == "*":
                i += 2
                while i + 1 < len(text) and not (text[i] == "*" and text[i + 1] == "/"):
                    i += 1
                i += 2
                continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
        elif ch in "\"'":
            quote = ch
            i += 1
            while i < len(text):
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    break
                i += 1
        i += 1
    raise ValueError("unbalanced '{' in function body")


def split_params(params_raw: str) -> List[str]:
    params_raw = params_raw.strip()
    if not params_raw:
        return []
    parts: List[str] = []
    cur: List[str] = []
    depth = 0
    for ch in params_raw:
        if ch == "(":
            depth += 1
            cur.append(ch)
        elif ch == ")":
            depth -= 1
            cur.append(ch)
        elif ch == "," and depth == 0:
            part = "".join(cur).strip()
            if part:
                parts.append(part)
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        parts.append(tail)
    return parts


def parse_bi_param(param: str) -> Optional[str]:
    m = BI_PARAM_RE.fullmatch(param.strip())
    if not m:
        return None
    if "__REGA0" in param:
        return "A0"
    if "__REGA1" in param:
        return "A1"
    return None


def camel_method_name(name: str, rename: Dict[str, str]) -> str:
    if name in rename:
        return rename[name]
    return name[0].lower() + name[1:]


def method_params_text(rest_params: Sequence[str]) -> str:
    if not rest_params:
        return ""
    converted = []
    for p in rest_params:
        if "__REGD7" in p and "RGBFTYPE" in p and "RGBFTYPE_REG" not in p:
            p = re.sub(r"\bRGBFTYPE\b", "RGBFTYPE_REG", p)
        converted.append(p)
    return ", ".join(converted)


def extract_param_names(params: Sequence[str]) -> List[str]:
    names: List[str] = []
    for p in params:
        p = p.strip()
        m = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*$", p)
        if m:
            names.append(m.group(1))
            continue
        m2 = re.search(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*$", p)
        names.append(m2.group(1) if m2 else "arg")
    return names


def body_has_bi_this(body: str) -> bool:
    return re.search(r"\bBoardInfo\s*\*\s*bi\s*=\s*this\s*;", body) is not None


def insert_bi_this(body: str) -> str:
    if body_has_bi_this(body):
        return body
    inner = body[1:-1]
    m = re.search(r"\n([ \t]+)", inner)
    indent = m.group(1) if m else "    "
    if inner.startswith("\n"):
        insert = f"\n{indent}BoardInfo *bi = this;"
    else:
        insert = f"\n{indent}BoardInfo *bi = this;\n"
    return "{" + insert + inner + "}"


def find_hook_functions(text: str, skip: set[str]) -> List[HookFunction]:
    hooks: List[HookFunction] = []
    for m in HOOK_HEAD_RE.finditer(text):
        name = m.group("name")
        if name in skip:
            continue
        open_paren = m.end() - 1
        try:
            close_paren = find_matching_paren(text, open_paren)
            body_start, body_end = find_function_body(text, close_paren)
        except ValueError:
            continue
        params_raw = text[open_paren + 1 : close_paren]
        params = split_params(params_raw)
        if not params:
            continue
        bi_reg = parse_bi_param(params[0])
        if bi_reg is None:
            continue
        decl_start = m.start("indent")
        hooks.append(
            HookFunction(
                start=decl_start,
                end=body_end,
                indent=m.group("indent") or "",
                is_static=bool(m.group("static")),
                ret_type=m.group("ret"),
                name=name,
                params_raw=params_raw,
                body=text[body_start:body_end],
                bi_reg=bi_reg,
            )
        )
    hooks.sort(key=lambda h: h.start)
    return hooks


def build_method(hook: HookFunction, driver_class: str, rename: Dict[str, str]) -> str:
    params = split_params(hook.params_raw)
    rest = params[1:]
    method_name = camel_method_name(hook.name, rename)
    sig_params = method_params_text(rest)
    sig = f"{hook.ret_type} ASM {driver_class}::{method_name}({sig_params})"
    body = insert_bi_this(hook.body)
    return f"{sig}\n{body}"


def build_trampoline(
    hook: HookFunction,
    as_fn: str,
    rename: Dict[str, str],
) -> str:
    params = split_params(hook.params_raw)
    rest = params[1:]
    method_name = camel_method_name(hook.name, rename)
    tramp_name = rename.get(hook.name, hook.name)
    sig = f"static {hook.ret_type} ASM {tramp_name}({hook.params_raw})"
    if hook.bi_reg == "A1" and not rest:
        call = f"return {as_fn}(bi)->{method_name}();"
    elif hook.ret_type == "void":
        arg_names = ", ".join(extract_param_names(rest))
        call = f"{as_fn}(bi)->{method_name}({arg_names});"
    else:
        arg_names = ", ".join(extract_param_names(rest))
        call = f"return {as_fn}(bi)->{method_name}({arg_names});"
    return f"{sig}\n{{\n    {call}\n}}"


def remove_forward_decls(text: str, converted_names: set[str]) -> str:
    out: List[str] = []
    pos = 0
    for m in FORWARD_HEAD_RE.finditer(text):
        name = m.group("name")
        if name not in converted_names:
            continue
        open_paren = m.end() - 1
        try:
            close_paren = find_matching_paren(text, open_paren)
        except ValueError:
            continue
        j = close_paren + 1
        while j < len(text) and text[j].isspace():
            j += 1
        if j >= len(text) or text[j] != ";":
            continue
        j += 1
        while j < len(text) and text[j] in " \t":
            j += 1
        if j < len(text) and text[j] == "\n":
            j += 1
        out.append(text[pos : m.start()])
        out.append(f"{m.group('indent')}/* removed forward decl: {name} (now {driver_class_note(name)}) */\n")
        pos = j
    out.append(text[pos:])
    return "".join(out)


def driver_class_note(name: str) -> str:
    return f"{name} method"


def apply_p96_hook(text: str, trampoline_names: set[str]) -> str:
    lines = text.splitlines(keepends=True)
    out: List[str] = []
    in_initchip = False
    brace_depth = 0
    for line in lines:
        if not in_initchip and INITCHIP_RE.search(line):
            in_initchip = True
            brace_depth = line.count("{") - line.count("}")
            out.append(line)
            continue
        if in_initchip:
            brace_depth += line.count("{") - line.count("}")
            m = P96_ASSIGN_RE.match(line.rstrip("\n"))
            if m and m.group(3) in trampoline_names:
                indent, field, func = m.group(1), m.group(2), m.group(3)
                out.append(f"{indent}P96_HOOK(bi->{field}, {func});\n")
            else:
                out.append(line)
            if brace_depth <= 0:
                in_initchip = False
            continue
        out.append(line)
    return "".join(out)


def emit_trampolines_block(trampolines: Sequence[str]) -> str:
    if not trampolines:
        return ""
    parts = ["/* P96 BoardInfo entry stubs */", ""]
    parts.extend(trampolines)
    parts.append("")
    return "\n".join(parts)


def convert_source(
    text: str,
    *,
    driver_class: str,
    as_fn: str,
    rename: Dict[str, str],
    skip: set[str],
    emit_trampolines_at: str,
    p96_hook: bool,
) -> str:
    skip = set(skip) | {"InitChip"}
    hooks = find_hook_functions(text, skip)
    if not hooks:
        return text

    converted_names = {h.name for h in hooks}
    trampolines = [build_trampoline(h, as_fn, rename) for h in hooks]

    chunks: List[str] = []
    pos = 0
    for hook in hooks:
        chunks.append(text[pos:hook.start])
        chunks.append(build_method(hook, driver_class, rename))
        pos = hook.end
    chunks.append(text[pos:])
    result = "".join(chunks)

    # Drop forward declarations for converted static hooks.
    result = remove_forward_decls(result, converted_names)

    if p96_hook:
        tramp_names = set()
        for h in hooks:
            tramp_names.add(rename.get(h.name, h.name))
        result = apply_p96_hook(result, tramp_names)

    tramp_block = emit_trampolines_block(trampolines)
    if not tramp_block:
        return result

    marker = TRAMPOLINE_MARKER
    if emit_trampolines_at.upper() == "END":
        if result.endswith("\n"):
            return result + tramp_block
        return result + "\n" + tramp_block
    if marker in result:
        return result.replace(marker, tramp_block + marker, 1)

    m = INITCHIP_RE.search(result)
    if m:
        insert_at = m.start()
        return result[:insert_at] + tramp_block + result[insert_at:]

    if result.endswith("\n"):
        return result + tramp_block
    return result + "\n" + tramp_block


def parse_rename(items: Sequence[str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for item in items:
        if "=" not in item:
            raise argparse.ArgumentTypeError(f"invalid --rename {item!r}, expected Name=method")
        left, right = item.split("=", 1)
        left, right = left.strip(), right.strip()
        if not left or not right:
            raise argparse.ArgumentTypeError(f"invalid --rename {item!r}")
        out[left] = right
    return out


def self_test() -> None:
    sample = """
static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))
{
    REGBASE();
    waitFifo(bi, 16);
}

static void ASM SetGC(__REGA0(struct BoardInfo *bi), __REGA1(struct ModeInfo *mi), __REGD0(BOOL border))
{
    REGBASE();
    (void)mi;
    (void)border;
}

ULONG ASM VBlankInterruptHandler(__REGA1(struct BoardInfo *bi))
{
    return 1;
}

BOOL InitChip(__REGA0(struct BoardInfo *bi))
{
    bi->WaitBlitter = WaitBlitter;
    bi->SetGC = SetGC;
    return TRUE;
}
"""
    out = convert_source(
        sample,
        driver_class="Mach32Driver",
        as_fn="asMach32",
        rename={"VBlankInterruptHandler": "interruptServer"},
        skip=set(),
        emit_trampolines_at="END",
        p96_hook=False,
    )
    checks = [
        "void ASM Mach32Driver::waitBlitter()",
        "BoardInfo *bi = this;",
        "void ASM Mach32Driver::setGC(__REGA1(struct ModeInfo *mi), __REGD0(BOOL border))",
        "ULONG ASM Mach32Driver::interruptServer()",
        "static void ASM WaitBlitter(__REGA0(struct BoardInfo *bi))",
        "asMach32(bi)->waitBlitter();",
        "static void ASM SetGC(",
        "BOOL InitChip(__REGA0(struct BoardInfo *bi))",
    ]
    missing = [c for c in checks if c not in out]
    if missing:
        print("self-test FAILED, missing:", missing, file=sys.stderr)
        print(out, file=sys.stderr)
        raise SystemExit(1)
    print("self-test OK")


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Convert P96 chip ASM hooks to C++ driver methods + trampolines.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s mach32/chip_mach32.c --driver-class Mach32Driver --as-fn asMach32 -o chip_mach32.cpp
  %(prog)s --self-test
  head -200 mach32/chip_mach32.c | %(prog)s - --driver-class Mach32Driver --as-fn asMach32
""",
    )
    p.add_argument(
        "input",
        nargs="?",
        help="source file (default: stdin)",
    )
    p.add_argument(
        "-o",
        "--output",
        help="output file (default: stdout)",
    )
    p.add_argument(
        "--driver-class",
        required=False,
        default="Mach32Driver",
        help="C++ driver class name (default: Mach32Driver)",
    )
    p.add_argument(
        "--as-fn",
        default="asMach32",
        help="cast/helper used in trampolines (default: asMach32)",
    )
    p.add_argument(
        "--rename",
        action="append",
        default=[],
        metavar="Hook=method",
        help="rename hook to method (repeatable), e.g. VBlankInterruptHandler=interruptServer",
    )
    p.add_argument(
        "--skip",
        action="append",
        default=[],
        metavar="NAME",
        help="do not convert this hook (repeatable); InitChip is always skipped",
    )
    p.add_argument(
        "--emit-trampolines-at",
        default="InitChip",
        choices=["InitChip", "END", "marker"],
        help="where to emit trampolines: before InitChip, at /* CXX_TRAMPOLINES */, or END",
    )
    p.add_argument(
        "--p96-hook",
        action="store_true",
        help="rewrite bi->Field = Func; inside InitChip to P96_HOOK(...)",
    )
    p.add_argument(
        "--self-test",
        action="store_true",
        help="run built-in conversion checks and exit",
    )
    return p


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if args.self_test:
        self_test()
        return 0

    if args.input and args.input != "-":
        with open(args.input, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    rename = parse_rename(args.rename)
    emit_at = "marker" if args.emit_trampolines_at == "marker" else args.emit_trampolines_at

    out = convert_source(
        text,
        driver_class=args.driver_class,
        as_fn=args.as_fn,
        rename=rename,
        skip=set(args.skip),
        emit_trampolines_at=emit_at,
        p96_hook=args.p96_hook,
    )

    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(out)
    else:
        sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
