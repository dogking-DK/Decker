#!/usr/bin/env python3
import argparse, subprocess, sys, shutil, re
from pathlib import Path

ENTRY_REGEX = re.compile(
    r'\[\s*shader\s*\(\s*"(vertex|fragment|compute|geometry|task|mesh|raygeneration|miss|closesthit|anyhit|intersection|callable)"\s*\)\s*\]\s*[^\{\(;]*\b([A-Za-z_]\w*)\s*\('
)

def find_slangc():
    from os import environ
    if "SLANGC" in environ and Path(environ["SLANGC"]).exists():
        return environ["SLANGC"]
    for name in ("slangc", "slangc.exe", "slang"):
        p = shutil.which(name)
        if p: return p
    print("[ERR] slangc not found in PATH. Set SLANGC or install slangc.", file=sys.stderr)
    sys.exit(1)

def run(cmd):
    print("  $", " ".join(cmd))
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.stdout.strip(): print(r.stdout)
    if r.returncode != 0:
        raise SystemExit(r.returncode)

def detect_entries(text: str):
    return [(m.group(1), m.group(2)) for m in ENTRY_REGEX.finditer(text)]

def compile_entry(slangc, src, dst, entry, profile, include_dirs):
    cmd = [slangc, str(src), "-target", "spirv", "-profile", profile, "-entry", entry, "-o", str(dst)]
    for inc in include_dirs:
        cmd += ["-I", str(inc)]
    run(cmd)

def main():
    ap = argparse.ArgumentParser(description="Compile ./slang/** to ./spv/** (mirror structure)")
    ap.add_argument("--src", default="slang", help="source root (default: ./slang)")
    ap.add_argument("--out", default="spv", help="output root (default: ./spv)")
    ap.add_argument("--profile", default="spirv_1_5", help="SPIR-V profile (default: spirv_1_5)")
    ap.add_argument("--entry", default=None, help="fallback entry if no [shader(...)] found")
    ap.add_argument("--all-entries", action="store_true",
                    help="also emit extra files for other entries as name.slang@entry.spv (default: off)")
    args = ap.parse_args()

    slangc = find_slangc()
    src_root = Path(args.src).resolve()
    out_root = Path(args.out).resolve()
    out_root.mkdir(parents=True, exist_ok=True)

    files = list(src_root.rglob("*.slang"))
    if not files:
        print(f"[INFO] No .slang files under {src_root}")
        return

    print(f"[INFO] {len(files)} Slang files. Tool=slangc ({slangc})")
    for f in files:
        text = f.read_text(encoding="utf-8", errors="ignore")
        entries = detect_entries(text)
        print(f"[SCAN] {f.relative_to(src_root)} -> entries={[(st,ep) for st,ep in entries]}")

        rel_parent = f.parent.relative_to(src_root)
        dst_dir = out_root / rel_parent
        dst_dir.mkdir(parents=True, exist_ok=True)

        # 主输出（严格满足：源文件名后（含后缀）追加 .spv）
        main_out = dst_dir / (f.name + ".spv")

        include_dirs = [src_root, f.parent]  # 允许相对 include

        if entries:
            # 用第一个入口作为主输出
            st0, ep0 = entries[0]
            print(f"[BUILD] {f.relative_to(src_root)} -> {main_out.relative_to(out_root)} (entry={ep0}, stage={st0})")
            compile_entry(slangc, f, main_out, ep0, args.profile, include_dirs)

            # 可选：其它入口额外导出（避免覆盖主输出）
            if args.all-entries and len(entries) > 1:
                for st, ep in entries[1:]:
                    extra = dst_dir / (f"{f.name}@{ep}.spv")
                    print(f"[BUILD+] {f.relative_to(src_root)} -> {extra.relative_to(out_root)} (entry={ep}, stage={st})")
                    compile_entry(slangc, f, extra, ep, args.profile, include_dirs)
        else:
            if args.entry:
                print(f"[BUILD] {f.relative_to(src_root)} -> {main_out.relative_to(out_root)} (fallback entry={args.entry})")
                compile_entry(slangc, f, main_out, args.entry, args.profile, include_dirs)
            else:
                print(f"[WARN] {f.relative_to(src_root)}: no [shader(...)] and no --entry; skipped.")

    print(f"[OK] Output mirrored under: {out_root}")

if __name__ == "__main__":
    main()
