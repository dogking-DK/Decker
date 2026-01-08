#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
convert_to_utf8_bom.py

Recursively scan source files and convert to UTF-8 with BOM (UTF-8-SIG):
- UTF-8 without BOM -> add BOM
- UTF-8 with BOM -> keep (no change)
- GB-family encodings (GB2312/GBK/GB18030 etc.) -> UTF-8 with BOM
  Only when UTF-8 decoding fails but GB18030 decoding succeeds.

Default: dry-run (preview only)
Use --no-dry-run to actually modify files.
"""

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, List, Set

UTF8_BOM = b"\xef\xbb\xbf"


@dataclass
class DetectResult:
    path: Path
    action: str               # "convert" | "skip" | "unsupported"
    detected: str             # "utf-8" | "utf-8-bom" | "gb-family" | "binary" | "utf-16" | ...
    reason: str
    new_bytes: Optional[bytes]  # bytes to write if action == "convert"


def is_probably_binary(data: bytes) -> bool:
    # For .cpp/.h normally not binary; NUL byte is a strong signal.
    return b"\x00" in data


def detect_and_prepare_convert(path: Path, add_bom_for_utf8: bool = True) -> DetectResult:
    data = path.read_bytes()

    if not data:
        # 空文件是否要加 BOM 看个人习惯。这里默认不动（更保守）。
        return DetectResult(path, "skip", "empty", "empty file; skip", None)

    if is_probably_binary(data):
        return DetectResult(path, "skip", "binary", "contains NUL byte; skip", None)

    # Detect UTF-16 BOM (unsupported)
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return DetectResult(path, "unsupported", "utf-16", "UTF-16 BOM detected; not converting", None)

    # 1) Already UTF-8 with BOM -> keep
    if data.startswith(UTF8_BOM):
        try:
            _ = data.decode("utf-8-sig", errors="strict")
        except UnicodeDecodeError as e:
            return DetectResult(path, "unsupported", "utf-8-bom?", f"has UTF-8 BOM but decode failed: {e}", None)
        return DetectResult(path, "skip", "utf-8-bom", "already UTF-8 with BOM; keep", None)

    # 2) Try UTF-8 (no BOM)
    try:
        text_utf8 = data.decode("utf-8", errors="strict")
        if add_bom_for_utf8:
            new_data = text_utf8.encode("utf-8-sig")  # adds BOM
            # If original had no BOM, new_data will differ (BOM added)
            return DetectResult(path, "convert", "utf-8", "valid UTF-8 but no BOM -> add BOM", new_data)
        else:
            return DetectResult(path, "skip", "utf-8", "valid UTF-8; keep", None)
    except UnicodeDecodeError:
        pass

    # 3) UTF-8 fails -> try GB18030 strict (covers GBK/GB2312 subsets too)
    try:
        text_gb = data.decode("gb18030", errors="strict")
    except UnicodeDecodeError as e:
        return DetectResult(path, "unsupported", "unknown", f"not UTF-8, not GB18030: {e}", None)

    new_data = text_gb.encode("utf-8-sig")  # UTF-8 with BOM
    return DetectResult(path, "convert", "gb-family", "UTF-8 decode failed but GB18030 succeeded -> convert to UTF-8 with BOM", new_data)


def iter_target_files(root: Path, exts: Set[str]) -> List[Path]:
    files: List[Path] = []
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() in exts:
            files.append(p)
    return files


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Detect and convert GB-family / UTF-8-no-BOM C/C++ source files to UTF-8 with BOM. Default is dry-run."
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Root directory to scan (default: current directory).",
    )
    parser.add_argument(
        "--ext",
        default=".cpp,.h,hpp",
        help="Comma-separated file extensions to include (default: .cpp,.h). Example: --ext .cpp,.h,.hpp,.c",
    )
    parser.add_argument(
        "--no-dry-run",
        action="store_true",
        help="Actually modify files. Default is dry-run (preview only).",
    )
    parser.add_argument(
        "--backup",
        action="store_true",
        help="Create .bak backup when converting (only works with --no-dry-run).",
    )
    parser.add_argument(
        "--list-all",
        action="store_true",
        help="List all scanned files, including skipped ones. Default prints only files that would change or unsupported.",
    )
    parser.add_argument(
        "--fail-on-unsupported",
        action="store_true",
        help="Return non-zero exit code if unsupported/unknown-encoding files are found.",
    )

    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    if not root.exists():
        print(f"[ERROR] Root path does not exist: {root}", file=sys.stderr)
        return 2

    exts = {e.strip().lower() for e in args.ext.split(",") if e.strip()}
    if not exts:
        print("[ERROR] No extensions provided.", file=sys.stderr)
        return 2

    dry_run = not args.no_dry_run
    files = iter_target_files(root, exts)

    if not files:
        print(f"[INFO] No target files found under: {root} with extensions {sorted(exts)}")
        return 0

    converted = 0
    would_convert = 0
    skipped = 0
    unsupported = 0

    print(f"[INFO] Scan root: {root}")
    print(f"[INFO] Extensions: {sorted(exts)}")
    print(f"[INFO] Target: UTF-8 with BOM (utf-8-sig)")
    print(f"[INFO] Mode: {'DRY-RUN (preview only)' if dry_run else 'APPLY (write files)'}")
    if not dry_run and args.backup:
        print("[INFO] Backup: enabled (.bak)")

    for p in files:
        res = detect_and_prepare_convert(p, add_bom_for_utf8=True)

        show = args.list_all or res.action in ("convert", "unsupported")
        if show:
            tag = "OK"
            if res.action == "convert":
                tag = "CHANGE" if dry_run else "WRITE"
            elif res.action == "unsupported":
                tag = "UNSUPPORTED"
            elif res.action == "skip":
                tag = "SKIP"
            print(f"[{tag}] {res.path}")
            print(f"      detected: {res.detected}")
            print(f"      reason  : {res.reason}")

        if res.action == "skip":
            skipped += 1
            continue
        if res.action == "unsupported":
            unsupported += 1
            continue
        if res.action == "convert":
            would_convert += 1
            if dry_run:
                continue

            assert res.new_bytes is not None

            if args.backup:
                bak = res.path.with_suffix(res.path.suffix + ".bak")
                if not bak.exists():
                    bak.write_bytes(res.path.read_bytes())

            res.path.write_bytes(res.new_bytes)
            converted += 1

    print("\n[SUMMARY]")
    print(f"  scanned      : {len(files)}")
    print(f"  skipped      : {skipped}")
    print(f"  unsupported  : {unsupported}")
    print(f"  would_change : {would_convert}")
    if not dry_run:
        print(f"  changed      : {converted}")

    if args.fail_on_unsupported and unsupported > 0:
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
