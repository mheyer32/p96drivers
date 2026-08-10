#!/usr/bin/env python3
"""Mechanical REGBASE/W_*/R_* → aperture migration for S3/AT3D C++ sources."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Balanced-paren argument splitter for macro calls.
def split_args(argstr: str) -> list[str]:
    args = []
    depth = 0
    cur = []
    i = 0
    while i < len(argstr):
        c = argstr[i]
        if c == '(':
            depth += 1
            cur.append(c)
        elif c == ')':
            depth -= 1
            cur.append(c)
        elif c == ',' and depth == 0:
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    if cur or args:
        args.append(''.join(cur).strip())
    return args


def replace_call(text: str, name: str, replacer, arity: int | None = None) -> str:
    """Replace NAME(args) with replacer(args_list) -> str."""
    out = []
    i = 0
    n = len(text)
    pat = re.compile(r'\b' + re.escape(name) + r'\s*\(')
    while i < n:
        m = pat.search(text, i)
        if not m:
            out.append(text[i:])
            break
        # Skip if on a // comment line
        line_start = text.rfind('\n', 0, m.start()) + 1
        if '//' in text[line_start:m.start()]:
            out.append(text[i:m.end()])
            i = m.end()
            continue
        out.append(text[i:m.start()])
        j = m.end()
        depth = 1
        while j < n and depth:
            if text[j] == '(':
                depth += 1
            elif text[j] == ')':
                depth -= 1
            j += 1
        argstr = text[m.end() : j - 1]
        args = split_args(argstr)
        if arity is not None and len(args) != arity:
            out.append(text[m.start():j])
        else:
            try:
                out.append(replacer(args))
            except (IndexError, TypeError):
                out.append(text[m.start():j])
        i = j
    return ''.join(out)


def transform(text: str, chip: str) -> str:
    if chip == 's3':
        as_drv = 'asS3'
        io_id = 'S3_IO_ID'
        mmio_id = 'S3_MMIO_ID'
        vga_ty = 'VgaIo'
        mmio_ty = 'S3Mmio'
        vga_getter = f'{as_drv}(bi)->vga()'
    else:
        as_drv = 'asAt3d'
        io_id = 'VGA_ID'
        mmio_id = 'AT3D_MMIO_ID'
        vga_ty = 'VgaIo'
        mmio_ty = 'At3dMmio'
        vga_getter = f'{as_drv}(bi)->vga()'

    # Base setup — REGBASE is always the shared VGA aperture
    text = re.sub(
        r'\bREGBASE\s*\(\s*\)\s*;',
        f'{vga_ty} vga = {vga_getter};',
        text,
    )
    text = re.sub(
        r'\bMMIOBASE\s*\(\s*\)\s*;',
        f'{mmio_ty} mmio = {as_drv}(bi)->mmio();',
        text,
    )
    text = re.sub(
        r'\bLEGACYIOBASE\s*\(\s*\)\s*;',
        f'{vga_ty} vga = {as_drv}(bi)->legacyVga();',
        text,
    )

    # VGA indexed
    text = replace_call(text, 'W_CR', lambda a: f'vga.writeCR({a[0]}, {a[1]})', 2)
    text = replace_call(text, 'R_CR', lambda a: f'vga.readCR({a[0]})', 1)
    text = replace_call(text, 'W_CR_MASK', lambda a: f'vga.writeCRMask({a[0]}, {a[1]}, {a[2]})', 3)
    text = replace_call(text, 'W_SR', lambda a: f'vga.writeSR({a[0]}, {a[1]})', 2)
    text = replace_call(text, 'R_SR', lambda a: f'vga.readSR({a[0]})', 1)
    text = replace_call(text, 'W_SR_MASK', lambda a: f'vga.writeSRMask({a[0]}, {a[1]}, {a[2]})', 3)
    text = replace_call(text, 'W_GR', lambda a: f'vga.writeGR({a[0]}, {a[1]})', 2)
    text = replace_call(text, 'R_GR', lambda a: f'vga.readGR({a[0]})', 1)
    text = replace_call(text, 'W_AR', lambda a: f'vga.writeAR({a[0]}, {a[1]})', 2)
    text = replace_call(text, 'R_AR', lambda a: f'vga.readAR({a[0]})', 1)
    text = replace_call(text, 'W_MISC_MASK', lambda a: f'vga.writeMiscMask({a[0]}, {a[1]})', 2)

    text = replace_call(
        text,
        'W_CR_OVERFLOW1',
        lambda a: f'vga.writeCROverflow1({", ".join(a)})',
        7,
    )
    text = replace_call(
        text,
        'W_CR_OVERFLOW2_ULONG',
        lambda a: f'vga.writeCROverflow2U({", ".join(a)})',
        10,
    )
    text = replace_call(
        text,
        'W_CR_OVERFLOW2',
        lambda a: f'vga.writeCROverflow2({", ".join(a)})',
        10,
    )
    text = replace_call(
        text,
        'W_CR_OVERFLOW3',
        lambda a: f'vga.writeCROverflow3({", ".join(a)})',
        13,
    )

    # Flat VGA byte ports
    text = replace_call(text, 'W_REG', lambda a: f'vga.writeB(VGA_ID({a[0]}), {a[1]})', 2)
    text = replace_call(text, 'R_REG', lambda a: f'vga.readB(VGA_ID({a[0]}))', 1)
    text = replace_call(
        text, 'W_REG_MASK', lambda a: f'vga.writeMaskB(VGA_ID({a[0]}), {a[1]}, {a[2]})', 3
    )

    # S3 GE I/O (Vision864 etc.) — separate S3Io
    if chip == 's3':
        text = replace_call(text, 'W_IO_W', lambda a: f'io.writeW(S3_IO_ID({a[0]}), {a[1]})', 2)
        text = replace_call(text, 'R_IO_W', lambda a: f'io.readW(S3_IO_ID({a[0]}))', 1)
        text = replace_call(text, 'W_IO_L', lambda a: f'io.writeL(S3_IO_ID({a[0]}), {a[1]})', 2)
        text = replace_call(text, 'R_IO_L', lambda a: f'io.readL(S3_IO_ID({a[0]}))', 1)
        text = replace_call(
            text, 'W_IO_MASK_W', lambda a: f'io.writeMaskW(S3_IO_ID({a[0]}), {a[1]}, {a[2]})', 3
        )

    # MMIO
    text = replace_call(text, 'W_MMIO_B', lambda a: f'mmio.writeB({mmio_id}({a[0]}), {a[1]})', 2)
    text = replace_call(text, 'R_MMIO_B', lambda a: f'mmio.readB({mmio_id}({a[0]}))', 1)
    text = replace_call(
        text, 'W_MMIO_MASK_B', lambda a: f'mmio.writeMaskB({mmio_id}({a[0]}), {a[1]}, {a[2]})', 3
    )
    text = replace_call(text, 'W_MMIO_W', lambda a: f'mmio.writeW({mmio_id}({a[0]}), {a[1]})', 2)
    text = replace_call(text, 'R_MMIO_W', lambda a: f'mmio.readW({mmio_id}({a[0]}))', 1)
    text = replace_call(
        text, 'W_MMIO_MASK_W', lambda a: f'mmio.writeMaskW({mmio_id}({a[0]}), {a[1]}, {a[2]})', 3
    )
    text = replace_call(text, 'W_MMIO_L', lambda a: f'mmio.writeL({mmio_id}({a[0]}), {a[1]})', 2)
    text = replace_call(text, 'R_MMIO_L', lambda a: f'mmio.readL({mmio_id}({a[0]}))', 1)
    text = replace_call(
        text, 'W_MMIO_NOSWAP_L', lambda a: f'mmio.writeLRaw({mmio_id}({a[0]}), {a[1]})', 2
    )
    text = replace_call(text, 'TST_MMIO_L', lambda a: f'mmio.testL({mmio_id}({a[0]}), {a[1]})', 2)

    # BEE8
    if chip == 's3':
        text = replace_call(
            text, 'W_BEE8', lambda a: f'{as_drv}(bi)->writeBee8({a[0]}, {a[1]})', 2
        )
        text = replace_call(text, 'R_BEE8', lambda a: f'{as_drv}(bi)->readBee8({a[0]})', 1)

    # Direct readReg/writeReg(RegBase, port[, val])
    def repl_readReg(a):
        if len(a) >= 2 and 'RegBase' in a[0]:
            return f'vga.readB(VGA_ID({a[1]}))'
        return None

    def repl_writeReg(a):
        if len(a) >= 3 and 'RegBase' in a[0]:
            return f'vga.writeB(VGA_ID({a[1]}), {a[2]})'
        return None

    def replace_maybe(text, name, fn):
        out = []
        i = 0
        n = len(text)
        pat = re.compile(r'\b' + re.escape(name) + r'\s*\(')
        while i < n:
            m = pat.search(text, i)
            if not m:
                out.append(text[i:])
                break
            out.append(text[i:m.start()])
            j = m.end()
            depth = 1
            while j < n and depth:
                if text[j] == '(':
                    depth += 1
                elif text[j] == ')':
                    depth -= 1
                j += 1
            args = split_args(text[m.end() : j - 1])
            rep = fn(args)
            if rep is None:
                out.append(text[m.start():j])
            else:
                out.append(rep)
            i = j
        return ''.join(out)

    text = replace_maybe(text, 'readReg', repl_readReg)
    text = replace_maybe(text, 'writeReg', repl_writeReg)

    # writeRegister(RegBase, port, val, name) / readRegister
    def repl_writeRegister(a):
        if len(a) >= 3 and 'RegBase' in a[0]:
            return f'io.writeB({io_id}({a[1]}), {a[2]})'
        if len(a) >= 3 and 'cv64CtrlReg' in a[0]:
            # writeRegister(cv64CtrlReg, 0, value, ...)
            return f'S3Io(getCardData(bi)->cv64CtrlReg).writeB({io_id}({a[1]}), {a[2]})'
        return None

    def repl_readRegister(a):
        if len(a) >= 2 and 'RegBase' in a[0]:
            return f'io.readB({io_id}({a[0] if False else a[1]}))'
        return None

    text = replace_maybe(text, 'writeRegister', repl_writeRegister)
    text = replace_maybe(text, 'readRegister', repl_readRegister)

    return text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('chip', choices=('s3', 'at3d'))
    ap.add_argument('files', nargs='+')
    args = ap.parse_args()
    for f in args.files:
        path = Path(f)
        old = path.read_text()
        new = transform(old, args.chip)
        if new != old:
            path.write_text(new)
            print(f'updated {path}')
        else:
            print(f'unchanged {path}')


if __name__ == '__main__':
    main()
