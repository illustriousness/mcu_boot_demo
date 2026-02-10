#!/usr/bin/env python3
"""
Post-link relocation checker for MCU PIE images.

Fail the build when relocation targets land in FLASH unexpectedly, which
typically indicates non-PIC input objects/libs or read-only pointer tables.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from functools import lru_cache
from typing import Iterable, List, Sequence, Tuple


@dataclass(frozen=True)
class Range:
    start: int
    end: int  # exclusive

    def contains(self, addr: int) -> bool:
        return self.start <= addr < self.end

    def __str__(self) -> str:
        return f"0x{self.start:08x}:0x{self.end:08x}"


@dataclass(frozen=True)
class Section:
    name: str
    start: int
    end: int  # exclusive

    def contains(self, addr: int) -> bool:
        return self.start <= addr < self.end


def run_cmd(cmd: Sequence[str]) -> str:
    p = subprocess.run(cmd, text=True, capture_output=True)
    if p.returncode != 0:
        msg = p.stderr.strip() or p.stdout.strip() or "unknown error"
        raise RuntimeError(f"command failed: {' '.join(cmd)}\n{msg}")
    return p.stdout


def parse_hex_u32(text: str) -> int:
    value = int(text, 0)
    if value < 0 or value > 0xFFFFFFFF:
        raise ValueError(f"u32 out of range: {text}")
    return value


def parse_range(spec: str) -> Range:
    if ":" not in spec:
        raise ValueError(f"invalid range '{spec}', expected start:end")
    left, right = spec.split(":", 1)
    start = parse_hex_u32(left.strip())
    end = parse_hex_u32(right.strip())
    if end <= start:
        raise ValueError(f"invalid range '{spec}', end must be > start")
    return Range(start, end)


def in_any_range(addr: int, ranges: Iterable[Range]) -> bool:
    return any(r.contains(addr) for r in ranges)


def parse_sections(readelf_s: str) -> List[Section]:
    # Example:
    # [ 1] .isr_vector PROGBITS 08000000 010000 000130 ...
    pat = re.compile(
        r"^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)\s+"
    )
    out: List[Section] = []
    for line in readelf_s.splitlines():
        m = pat.match(line)
        if not m:
            continue
        name = m.group(1)
        start = int(m.group(2), 16)
        size = int(m.group(3), 16)
        end = start + size
        if size == 0:
            continue
        out.append(Section(name=name, start=start, end=end))
    return out


def lookup_section(addr: int, sections: Sequence[Section]) -> str:
    for s in sections:
        if s.contains(addr):
            return s.name
    return "?"


def parse_relocations(readelf_r: str) -> List[Tuple[str, int, str]]:
    """
    Return list of (rel_section_name, offset, reloc_type).
    """
    rel_sec = ""
    out: List[Tuple[str, int, str]] = []
    sec_pat = re.compile(r"^Relocation section '([^']+)'")
    rel_pat = re.compile(r"^\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+(\S+)")
    for line in readelf_r.splitlines():
        ms = sec_pat.match(line)
        if ms:
            rel_sec = ms.group(1)
            continue
        mr = rel_pat.match(line)
        if not mr:
            continue
        offset = int(mr.group(1), 16)
        rel_type = mr.group(2)
        out.append((rel_sec, offset, rel_type))
    return out


def has_textrel(readelf_d: str) -> bool:
    for line in readelf_d.splitlines():
        if "(TEXTREL)" in line:
            return True
    return False


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(description="Check PIE relocation safety for MCU app ELF")
    ap.add_argument("--elf", required=True, help="Path to ELF")
    ap.add_argument(
        "--readelf",
        default="arm-none-eabi-readelf",
        help="readelf tool (default: arm-none-eabi-readelf)",
    )
    ap.add_argument(
        "--addr2line",
        default="arm-none-eabi-addr2line",
        help="addr2line tool (default: arm-none-eabi-addr2line)",
    )
    ap.add_argument(
        "--flash-range",
        action="append",
        default=["0x08000000:0x08040000"],
        help="FLASH range start:end (hex/dec), repeatable",
    )
    ap.add_argument(
        "--ram-range",
        action="append",
        default=["0x20000000:0x20010000"],
        help="RAM range start:end (hex/dec), repeatable",
    )
    ap.add_argument(
        "--allow-flash-reloc",
        action="append",
        default=[],
        help="Allowed FLASH relocation target range start:end, repeatable",
    )
    ap.add_argument(
        "--allow-textrel",
        action="store_true",
        help="Do not fail on TEXTREL dynamic tag",
    )
    ap.add_argument(
        "--max-report",
        type=int,
        default=30,
        help="Max violation lines to print (default: 30)",
    )
    args = ap.parse_args(argv)

    try:
        flash_ranges = [parse_range(x) for x in args.flash_range]
        ram_ranges = [parse_range(x) for x in args.ram_range]
        allow_flash = [parse_range(x) for x in args.allow_flash_reloc]
    except ValueError as e:
        print(f"[pic-check] bad range: {e}", file=sys.stderr)
        return 2

    try:
        sec_out = run_cmd([args.readelf, "-S", args.elf])
        rel_out = run_cmd([args.readelf, "-r", args.elf])
        dyn_out = run_cmd([args.readelf, "-d", args.elf])
    except RuntimeError as e:
        print(f"[pic-check] {e}", file=sys.stderr)
        return 2

    sections = parse_sections(sec_out)
    relocs = parse_relocations(rel_out)
    textrel = has_textrel(dyn_out)

    @lru_cache(maxsize=1024)
    def sym_loc(addr: int) -> str:
        p = subprocess.run(
            [args.addr2line, "-e", args.elf, "-f", "-C", f"0x{addr:08x}"],
            text=True,
            capture_output=True,
        )
        if p.returncode != 0:
            return "?:?"
        lines = [x.strip() for x in p.stdout.splitlines() if x.strip()]
        if len(lines) >= 2:
            return f"{lines[0]} @ {lines[1]}"
        if len(lines) == 1:
            return lines[0]
        return "?:?"

    violations: List[str] = []
    flash_rel_total = 0
    ram_rel_total = 0

    for rel_sec, offset, rel_type in relocs:
        if not rel_type.startswith("R_ARM_"):
            continue
        if in_any_range(offset, flash_ranges):
            flash_rel_total += 1
            if not in_any_range(offset, allow_flash):
                sec = lookup_section(offset, sections)
                where = sym_loc(offset)
                violations.append(
                    f"{rel_sec}: {rel_type} target=0x{offset:08x} sec={sec} {where}"
                )
        elif in_any_range(offset, ram_ranges):
            ram_rel_total += 1

    print(f"[pic-check] ELF: {args.elf}")
    print(
        f"[pic-check] reloc targets: flash={flash_rel_total} ram={ram_rel_total} total={len(relocs)}"
    )
    if textrel:
        print("[pic-check] dynamic tag: TEXTREL present")
    else:
        print("[pic-check] dynamic tag: TEXTREL not present")

    failed = False
    if textrel and not args.allow_textrel:
        failed = True
        print("[pic-check] error: TEXTREL present (unexpected for strict PIC policy)")

    if violations:
        failed = True
        print(f"[pic-check] error: {len(violations)} disallowed FLASH relocation targets")
        for line in violations[: max(0, args.max_report)]:
            print(f"  - {line}")
        if len(violations) > args.max_report:
            left = len(violations) - args.max_report
            print(f"  ... {left} more")

    if failed:
        print("[pic-check] FAIL")
        return 1

    print("[pic-check] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

