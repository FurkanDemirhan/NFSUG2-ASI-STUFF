#!/usr/bin/env python3
"""
Extract symbols, vtables, assertion strings, and source paths from PS2 ELF binaries.
Analyzes Alpha 10 and Demo builds to provide cross-referencing data.
"""

import os
import sys
import re
import json
from elftools.elf.elffile import ELFFile


def extract_elf_info(elf_path, output_json_path):
    print(f"[*] Analyzing PS2 ELF: {elf_path}")
    if not os.path.exists(elf_path):
        print(f"[-] File not found: {elf_path}")
        return False

    os.makedirs(os.path.dirname(os.path.abspath(output_json_path)), exist_ok=True)

    with open(elf_path, "rb") as f:
        data = f.read()
        f.seek(0)
        elf = ELFFile(f)

        sections = []
        vtables = []
        for s in elf.iter_sections():
            sec_info = {
                "name": s.name,
                "type": s["sh_type"],
                "addr": f"0x{s['sh_addr']:08X}",
                "size": s["sh_size"]
            }
            sections.append(sec_info)
            if s.name.startswith(".gnu.linkonce.d._vt$"):
                vtables.append({
                    "mangled": s.name,
                    "vtable_symbol": s.name[len(".gnu.linkonce.d."):],
                    "addr": f"0x{s['sh_addr']:08X}",
                    "size": s["sh_size"]
                })

    # Extract source files
    source_files = sorted(list(set(
        m.decode("latin1", errors="replace") 
        for m in re.findall(rb'([a-zA-Z0-9_\-\\\/]+\.(?:cpp|c|h))\b', data)
    )))

    # Extract assertion / debug strings
    # Typically: Assertion failed: ..., file ..., line ...
    assertions = sorted(list(set(
        m.decode("latin1", errors="replace") 
        for m in re.findall(rb'([A-Za-z0-9_]+(?:::|->)[A-Za-z0-9_]+\([^\)]*\))', data)
    )))

    # Extract all ASCII strings of length >= 6
    raw_strings = [
        s.decode("latin1", errors="replace")
        for s in re.findall(rb'[\x20-\x7E]{6,}', data)
    ]
    # Filter for interesting keywords
    keywords = ["debug", "unused", "test", "demo", "cheat", "cut", "camera", "menu", "race", "fng", "car", "world"]
    interesting_strings = sorted(list(set(
        s for s in raw_strings if any(k in s.lower() for k in keywords)
    )))

    result = {
        "elf": os.path.basename(elf_path),
        "file_size": len(data),
        "sections": sections,
        "vtables": vtables,
        "source_files": source_files,
        "interesting_strings_sample_count": len(interesting_strings),
        "interesting_strings": interesting_strings[:1000]
    }

    with open(output_json_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

    print(f"[+] Found {len(vtables)} vtables, {len(source_files)} source files, {len(interesting_strings)} keywords.")
    print(f"[+] Saved analysis to {output_json_path}")
    return True


if __name__ == "__main__":
    alpha10_elf = "Bin-idb-i64/SLUS_210.65__(Alpha10R_Bin).ELF"
    demo_elf = "Bin-idb-i64/SLUS_291.18_(PS2DEMO).ELF"
    
    extract_elf_info(alpha10_elf, "cache/ps2_alpha10_info.json")
    extract_elf_info(demo_elf, "cache/ps2_demo_info.json")
