#!/usr/bin/env python3
"""
Fast query tool for searching symbols across PC v1.2, GameCube, and PS2 builds.
Usage:
    python scripts/query_symbols.py <search_pattern>
"""

import sys
import json
import os
import re


def search_cache(query):
    query_lower = query.lower()
    print(f"=== Searching for '{query}' ===")

    # 1. PC Symbols
    pc_path = "cache/pc_v12_symbols.json"
    if os.path.exists(pc_path):
        with open(pc_path, "r", encoding="utf-8") as f:
            pc_data = json.load(f)
        matches = [s for s in pc_data["symbols"] if query_lower in s["name"].lower()]
        print(f"\n[PC v1.2 NTSC] Found {len(matches)} matches (showing top 30):")
        for m in matches[:30]:
            print(f"  {m['hex_ea']}: {m['name']}")
    else:
        print("\n[PC v1.2 NTSC] Cache not found. Run scripts/dump_ida_symbols.py first.")

    # 2. GameCube Symbols
    gc_path = "cache/gc_symbols.json"
    if os.path.exists(gc_path):
        with open(gc_path, "r", encoding="utf-8") as f:
            gc_data = json.load(f)
        matches = [s for s in gc_data["symbols"] if query_lower in s["name"].lower()]
        print(f"\n[GameCube NTSC] Found {len(matches)} matches (showing top 30):")
        for m in matches[:30]:
            print(f"  {m['hex_ea']}: {m['name']}")

    # 3. PS2 Alpha 10 & Demo Info
    for tag, path in [("PS2 Alpha 10", "cache/ps2_alpha10_info.json"), ("PS2 Demo", "cache/ps2_demo_info.json")]:
        if os.path.exists(path):
            with open(path, "r", encoding="utf-8") as f:
                ps2_data = json.load(f)
            vtables = [v for v in ps2_data.get("vtables", []) if query_lower in v["mangled"].lower()]
            sources = [s for s in ps2_data.get("source_files", []) if query_lower in s.lower()]
            strings = [s for s in ps2_data.get("interesting_strings", []) if query_lower in s.lower()]
            print(f"\n[{tag}]")
            if vtables:
                print(f"  Vtables ({len(vtables)}):")
                for v in vtables[:10]:
                    print(f"    {v['addr']}: {v['vtable_symbol']}")
            if sources:
                print(f"  Source files ({len(sources)}):")
                for s in sources[:10]:
                    print(f"    {s}")
            if strings:
                print(f"  Strings ({len(strings)}):")
                for s in strings[:15]:
                    print(f"    \"{s}\"")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python scripts/query_symbols.py <search_pattern>")
        sys.exit(1)
    search_cache(sys.argv[1])
