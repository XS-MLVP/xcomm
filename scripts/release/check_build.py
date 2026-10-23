#!/usr/bin/env python3
"""Check the actual CPU flags and coroutine configuration used for a wheel."""

import argparse
import json
from pathlib import Path
import shlex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    databases = list(args.build.rglob("compile_commands.json"))
    if not databases:
        raise SystemExit(f"No compile_commands.json under {args.build}")
    expected_arch = "-march=x86-64" if args.portable else "-march=native"
    expected_tune = "-mtune=generic" if args.portable else "-mtune=native"
    for database in databases:
        config = database.parent / "generated/xspcomm/xconfig.h"
        assert "#define ENABLE_XCOROUTINE true" in config.read_text(), config
        entries = json.loads(database.read_text())
        assert entries, database
        for entry in entries:
            command = entry.get("arguments") or shlex.split(entry["command"])
            assert [arg for arg in command if arg.startswith("-march=")] == [expected_arch], entry
            assert [arg for arg in command if arg.startswith("-mtune=")] == [expected_tune], entry
            assert "-fno-strict-aliasing" in command, entry
            if args.portable:
                assert "-D_GLIBCXX_USE_CXX11_ABI=1" in command, entry
        print(f"Checked {len(entries)} compile commands: {expected_arch} {expected_tune}")


if __name__ == "__main__":
    main()
