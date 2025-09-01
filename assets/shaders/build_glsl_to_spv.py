#!/usr/bin/env python3
import argparse, subprocess, sys, shutil
from pathlib import Path

STAGE_MAP = {
    ".vert": "vert", ".frag": "frag", ".comp": "comp", ".geom": "geom",
    ".tesc": "tesc", ".tese": "tese", ".mesh": "mesh", ".task": "task",
    ".rgen": "rgen", ".rmiss": "rmiss", ".rchit": "rchit",
    ".rahit": "rahit", ".rint": "rint", ".rcall": "rcall",
}

def find_tool():
    gv = shutil.which("glslangValidator")
    if gv: return ("glslangValidator", gv)
    gc = shutil.which("glslc")
    if gc: return ("glslc", gc)
    print("[ERR] Neither glslangValidator nor glslc found in PATH.", file=sys.stderr)
    sys.exit(1)

def run(cmd):
    print("  $", " ".join(cmd))
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.stdout.strip(): print(r.stdout)
    if r.returncode != 0:
        raise SystemExit(r.returncode)

def main():
    ap = argparse.ArgumentParser(description="Compile ./glsl/** to ./spv/** (mirror structure)")
    ap.add_argument("--src", default="glsl", help="source root (default: ./glsl)")
    ap.add_argument("--out", default="spv", help="output root (default: ./spv)")
    ap.add_argument("--target-env", default="vulkan1.4", help="for glslangValidator (default: vulkan1.4)")
    ap.add_argument("--recursive", action="store_true", help="recurse subdirectories (default: True)", default=True)
    args = ap.parse_args()

    src_root = Path(args.src).resolve()
    out_root = Path(args.out).resolve()
    out_root.mkdir(parents=True, exist_ok=True)

    kind, tool = find_tool()

    # 收集文件
    files = []
    globs = [f"*{ext}" for ext in STAGE_MAP.keys()]
    for ext in globs:
        files += list(src_root.rglob(ext))  # 我们默认递归

    if not files:
        print(f"[INFO] No GLSL files under {src_root}")
        return

    print(f"[INFO] {len(files)} GLSL files. Tool={kind} ({tool})")
    for f in files:
        stage = STAGE_MAP.get(f.suffix.lower())
        if not stage: 
            continue
        # 计算相对路径并在 spv 下镜像
        rel_parent = f.parent.relative_to(src_root)  # e.g. pbr/
        dst_dir = out_root / rel_parent
        dst_dir.mkdir(parents=True, exist_ok=True)
        dst = dst_dir / (f.name + ".spv")           # keep "name.ext.spv"

        print(f"[BUILD] {f.relative_to(src_root)} -> {dst.relative_to(out_root)} (stage={stage})")
        if kind == "glslangValidator":
            cmd = [tool, "-V", "-S", stage, "-o", str(dst), str(f)]
            if args.target_env:
                cmd[1:1] = ["--target-env", args.target_env]
            run(cmd)
        else:
            cmd = [tool, "-fshader-stage=" + stage, "-o", str(dst), str(f)]
            run(cmd)

    print(f"[OK] Output mirrored under: {out_root}")

if __name__ == "__main__":
    main()
    input("Press Enter to exit...")

