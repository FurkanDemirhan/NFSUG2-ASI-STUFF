#!/usr/bin/env python3
"""
Dump function addresses, names, and segments from IDA Pro .i64 databases using python-idb.
Saves extracted data to cache/<database_name>_symbols.json
"""

import os
import sys
import json
import time

try:
    import idb
except ImportError:
    print("Error: python-idb is not installed. Please run inside the project virtualenv.")
    sys.exit(1)


def dump_symbols(idb_path, output_json_path):
    print(f"[*] Opening IDA database: {idb_path}")
    start_time = time.time()
    
    if not os.path.exists(idb_path):
        print(f"[-] File not found: {idb_path}")
        return False

    os.makedirs(os.path.dirname(os.path.abspath(output_json_path)), exist_ok=True)

    with idb.from_file(idb_path) as db:
        api = idb.IDAPython(db)
        print(f"[+] Successfully loaded database in {time.time() - start_time:.2f}s. Extracting functions...")
        
        func_eas = list(api.idautils.Functions())
        total_funcs = len(func_eas)
        print(f"[+] Total functions identified: {total_funcs}")

        symbols = []
        named_count = 0

        for i, ea in enumerate(func_eas):
            name = api.idc.GetFunctionName(ea)
            is_named = not (name.startswith("sub_") or name.startswith("nullsub_") or name.startswith("j_sub_"))
            if is_named:
                named_count += 1
            
            symbols.append({
                "ea": ea,
                "hex_ea": f"0x{ea:08X}",
                "name": name,
                "is_named": is_named
            })

            if (i + 1) % 2000 == 0 or (i + 1) == total_funcs:
                print(f"    Progress: {i + 1}/{total_funcs} ({(i + 1) / total_funcs * 100:.1f}%) - {named_count} named")

        result = {
            "database": os.path.basename(idb_path),
            "total_functions": total_funcs,
            "named_functions": named_count,
            "symbols": symbols
        }

        with open(output_json_path, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)

        print(f"[+] Saved {named_count} named symbols ({total_funcs} total) to {output_json_path} in {time.time() - start_time:.2f}s")
        return True


if __name__ == "__main__":
    if len(sys.argv) < 3:
        # Default run for PC database
        pc_idb = "Bin-idb-i64/NFSUnderground2-v1.2-US.i64"
        out_json = "cache/pc_v12_symbols.json"
        dump_symbols(pc_idb, out_json)
    else:
        dump_symbols(sys.argv[1], sys.argv[2])
