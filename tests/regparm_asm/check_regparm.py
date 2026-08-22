#!/usr/bin/env python3
"""
Analyze m68k-amigaos objdump output for register-parameter probes.

Determines which data/address register each probe reads for its targeted parameter.
Prints a PASS/FAIL matrix and exits non-zero on unexpected failures.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# Formerly: g++ dropped __asm("dN") on enum parameters. Fixed in local
# amiga-gcc / amiga-gcc-13 (asmreg via type-variant copy, not attributes).
KNOWN_FAIL_TYPES: set[str] = set()

# a5 is the frame pointer; a4 is often reserved — probe anyway.
EXPECTED_SKIP = {
    ("c", "a5"),
    ("cxx", "a5"),
}

MOVE_TO_D0 = re.compile(
    r"move\.[bwl]\s+(?:(?:a7|sp)|([da])(\d+)),d0\b"
)
MOVE_D_TO_A5 = re.compile(r"move\.[bwl]\s+d(\d+),(-?\d+)\(a5\)")
MOVE_A_TO_A5 = re.compile(r"move\.[bwl]\s+a(\d+),(-?\d+)\(a5\)")
MOVE_D_TO_SP = re.compile(r"move\.[bwl]\s+d(\d+),-\(sp\)")
MOVE_A_TO_SP = re.compile(r"move\.[bwl]\s+a(\d+),-\(sp\)")
EXT_FROM_D = re.compile(r"extb?\.?[wl]?\s+d(\d+)\b")
ANDI_D0 = re.compile(r"andi\.[bwl]\s+#(?:0x)?[0-9a-fA-F]+,d0\b")
SYM_START = re.compile(r"^[0-9a-f]+\s+[0-9a-f]+\s+(_?[A-Za-z0-9_]+):\s*$")
SYM_START_ELF = re.compile(r"^[0-9a-f]+ <([^>]+)>:")


@dataclass
class ProbeResult:
    entry: dict
    actual_reg: Optional[str]
    note: str


def run_objdump(objdump: str, objfile: Path) -> str:
    proc = subprocess.run(
        [objdump, "-d", str(objfile)],
        check=True,
        capture_output=True,
        text=True,
    )
    return proc.stdout


def parse_functions(dump: str) -> dict[str, list[str]]:
    funcs: dict[str, list[str]] = {}
    cur_name: Optional[str] = None
    for raw in dump.splitlines():
        line = raw.strip()
        m = SYM_START.match(line) or SYM_START_ELF.match(line)
        if m:
            cur_name = m.group(1)
            funcs[cur_name] = []
            continue
        if cur_name is None or not line:
            continue
        if line.startswith("Disassembly"):
            continue
        # instruction lines: "0:\t4e75\trts" or "   0:	4e75           	rts"
        if ":" in line and "\t" in line:
            # keep the mnemonic part after the last tab if present
            parts = line.split("\t")
            funcs[cur_name].append(parts[-1].strip() if len(parts) > 1 else line)
    return funcs


def reg_from_move_to_d0(line: str) -> Optional[str]:
    m = MOVE_TO_D0.search(line)
    if not m:
        return None
    if m.group(1) is None:
        return "sp"
    return f"{m.group(1)}{m.group(2)}"


def analyze_prologue(lines: list[str], expect: str) -> tuple[Optional[str], str]:
    """
    Infer which register supplies the returned parameter.
    Optimized (-fomit-frame-pointer) code: move.* rN,d0 ; [ext/andi] ; rts
    """
    # Fast path: last move into d0 before rts
    last_src: Optional[str] = None
    for line in lines:
        if "rts" in line:
            break
        src = reg_from_move_to_d0(line)
        if src is not None:
            last_src = src
            continue
        # move.w dN,d0 already captured; ext.w d0 / andi on d0 keep last_src
        if EXT_FROM_D.search(line) or ANDI_D0.search(line):
            continue

    if last_src is not None:
        return last_src, "direct"

    # Bare rts / only andi on d0 -> value already in d0
    non_meta = [l for l in lines if l and not l.startswith("/")]
    if non_meta and non_meta[-1].endswith("rts"):
        interesting = [
            l
            for l in non_meta
            if not l.endswith("rts") and not ANDI_D0.search(l) and not EXT_FROM_D.search(l)
        ]
        if len(interesting) == 0:
            return "d0", "implicit-d0"

    # Frame-pointer path: collect stores to (a5)
    stores: list[tuple[int, str]] = []
    for line in lines:
        for rx, prefix in ((MOVE_D_TO_A5, "d"), (MOVE_A_TO_A5, "a")):
            m = rx.search(line)
            if m:
                stores.append((int(m.group(2)), f"{prefix}{m.group(1)}"))
        m = MOVE_D_TO_SP.search(line)
        if m:
            stores.append((-9999, f"d{m.group(1)}"))
        m = MOVE_A_TO_SP.search(line)
        if m:
            stores.append((-9999, f"a{m.group(1)}"))

    if stores:
        stores.sort(key=lambda x: x[0])
        return stores[-1][1], "frame"

    return None, "unknown"


def classify(entry: dict, actual: Optional[str], note: str) -> tuple[str, str]:
    expect = entry["expect_reg"]
    lang = entry["lang"]
    typ = entry["type"]
    reg = entry["reg"]

    key_skip = (lang, reg)
    enum_bug = lang == "cxx" and typ in KNOWN_FAIL_TYPES and expect.startswith("d")

    if actual == expect:
        if enum_bug:
            # With attribute ignored, -mregparm still places the first arg in d0.
            if expect == "d0":
                return "OK", f"reads d0 (mregparm coincident; enum attr still ignored)"
            return (
                "UNEXPECTED_OK",
                f"reads {expect} — g++ enum attribute worked?",
            )
        return "OK", f"reads {expect}" + (f" ({note})" if note else "")

    if enum_bug:
        got = actual or "?"
        return "KNOWN_FAIL", f"reads {got}, expected {expect}; g++ enum attribute bug"

    if key_skip in EXPECTED_SKIP:
        got = actual or "?"
        return "SKIP", f"reads {got}, expected {expect}; reserved frame/PIC reg"

    got = actual or "?"
    return "FAIL", f"reads {got}, expected {expect}" + (f" ({note})" if note else "")


def check_objects(
    manifest: list[dict],
    objects: list[Path],
    objdump: str,
) -> tuple[list[ProbeResult], int, int, int, int, int]:
    # symbol -> lines
    asm: dict[str, list[str]] = {}
    for obj in objects:
        funcs = parse_functions(run_objdump(objdump, obj))
        for name, lines in funcs.items():
            asm[name] = lines

    results: list[ProbeResult] = []
    ok = known = skip = fail = unexpected_ok = 0

    for entry in manifest:
        sym = entry["symbol"]
        lines = asm.get(sym)
        if lines is None:
            # objdump prefixes _
            lines = asm.get("_" + sym)
        if lines is None:
            results.append(ProbeResult(entry, None, "symbol not found"))
            status, _ = classify(entry, None, "missing")
        else:
            actual, note = analyze_prologue(lines, entry["expect_reg"])
            results.append(ProbeResult(entry, actual, note))
            status, _ = classify(entry, actual, note)

        if status == "OK":
            ok += 1
        elif status == "KNOWN_FAIL":
            known += 1
        elif status == "SKIP":
            skip += 1
        elif status == "UNEXPECTED_OK":
            unexpected_ok += 1
        else:
            fail += 1

    return results, ok, known, skip, fail, unexpected_ok


def print_matrix(results: list[ProbeResult]) -> None:
    print(f"{'LANG':<4} {'REG':<4} {'TYPE':<12} {'VARIANT':<18} {'STATUS':<12} DETAIL")
    print("-" * 90)
    for r in sorted(
        results,
        key=lambda x: (
            x.entry["lang"],
            x.entry["reg"],
            x.entry["type"],
            x.entry["variant"],
        ),
    ):
        e = r.entry
        status, detail = classify(e, r.actual_reg, r.note)
        print(
            f"{e['lang']:<4} {e['reg']:<4} {e['type']:<12} {e['variant']:<18} {status:<12} {detail}"
        )


def main() -> int:
    ap = argparse.ArgumentParser(description="Check m68k register parameter probes")
    ap.add_argument(
        "--manifest",
        type=Path,
        default=Path(__file__).resolve().parent / "generated" / "manifest.json",
    )
    ap.add_argument(
        "--objdump",
        default="m68k-amigaos-objdump",
        help="objdump executable",
    )
    ap.add_argument("objects", nargs="+", type=Path, help="object files to inspect")
    args = ap.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    results, ok, known, skip, fail, unexpected_ok = check_objects(
        manifest, args.objects, args.objdump
    )

    print_matrix(results)
    print()
    print(
        f"Summary: {ok} OK, {known} KNOWN_FAIL, {skip} SKIP, "
        f"{unexpected_ok} UNEXPECTED_OK, {fail} unexpected FAIL "
        f"(total {len(results)})"
    )

    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
