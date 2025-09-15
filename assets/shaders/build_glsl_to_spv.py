#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, subprocess, sys, shutil, tempfile
from pathlib import Path

# 映射：扩展名 -> 着色器阶段
STAGE_MAP = {
    ".vert": "vert", ".frag": "frag", ".comp": "comp", ".geom": "geom",
    ".tesc": "tesc", ".tese": "tese", ".mesh": "mesh", ".task": "task",
    ".rgen": "rgen", ".rmiss": "rmiss", ".rchit": "rchit",
    ".rahit": "rahit", ".rint": "rint", ".rcall": "rcall",
}

def find_tool():
    """在PATH中查找 glslangValidator 或 glslc。"""
    gv = shutil.which("glslangValidator")
    if gv: return ("glslangValidator", gv)
    gc = shutil.which("glslc")
    if gc: return ("glslc", gc)
    print("[ERR] Neither glslangValidator nor glslc found in PATH.", file=sys.stderr)
    sys.exit(1)

def run(cmd):
    """执行命令并打印输出，如果失败则退出。"""
    print("  $", " ".join(cmd))
    # 使用 text=True 和 utf-8 编码来捕获输出
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    output = r.stdout.strip() + "\n" + r.stderr.strip()
    if output.strip(): print(output)
    if r.returncode != 0:
        print(f"[ERR] Command failed with exit code {r.returncode}", file=sys.stderr)
        raise SystemExit(r.returncode)

# --- 新增：将源文件写出为 UTF-8 with BOM 的临时副本的函数 ---
def to_utf8_with_bom(src_path: Path, cache_root: Path, rel_path: Path) -> Path:
    """
    读取 src_path（尝试多种编码），然后写出为一个
    UTF-8 with BOM（utf-8-sig）格式的镜像副本，并返回新路径。
    BOM可以强制 glslangValidator/glslc 以UTF-8模式读取文件。
    """
    # 尝试用常见的编码读取源文件，优先UTF-8
    tried_encodings = ["utf-8", "utf-8-sig", "gbk", "cp936", "shift_jis"]
    content = None
    for enc in tried_encodings:
        try:
            content = src_path.read_text(encoding=enc)
            # 成功读取后立即跳出循环
            break
        except (UnicodeDecodeError, TypeError):
            continue
    
    # 如果所有尝试都失败了，作为最后手段，以二进制读取并替换无效字符
    if content is None:
        print(f"[WARN] Could not detect encoding for {src_path}. Reading as binary with replacements.", file=sys.stderr)
        content = src_path.read_bytes().decode("utf-8", errors="replace")

    # 构建临时副本的路径，并确保目录存在
    dst_path = cache_root / rel_path
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    
    # 关键：以 'utf-8-sig' 编码写入，这会自动在文件开头添加BOM
    # newline="" 防止 python 自动转换换行符，保持源文件原样
    dst_path.write_text(content, encoding="utf-8-sig", newline="")
    return dst_path

def build_cmd(kind, tool, stage, src, dst, args):
    """生成编译命令。为保证调试信息质量，推荐使用 glslangValidator。"""
    if kind == "glslangValidator":
        # -gVS: 生成源码级调试信息并嵌入源码
        # -Od:  Debug模式下关闭优化，保证行号准确
        cmd = [tool, "-V", "-S", stage, "-o", str(dst), str(src)]
        if args.target_env:
            cmd[1:1] = ["--target-env", args.target_env]
        if args.config == "debug":
            if args.embed_source:
                cmd.insert(1, "-gVS")
            if not args.keep_opt:
                cmd.insert(1, "-Od")
        else: # release
            if args.embed_source and args.debug_symbols_in_release:
                cmd.insert(1, "-gVS")
        return cmd
    else:  # glslc
        # -g:   生成调试信息
        # -O0:  关闭优化
        cmd = [tool, "-fshader-stage=" + stage, "-o", str(dst), str(src)]
        if args.target_env:
            cmd[1:1] = ["--target-env=" + args.target_env]
        if args.config == "debug":
            if args.embed_source:
                cmd.insert(1, "-g")
            if not args.keep_opt:
                cmd.insert(1, "-O0")
        else: # release
            if args.embed_source and args.debug_symbols_in_release:
                cmd.insert(1, "-g")
        return cmd

# --- 新增：可选的验证函数，用于检查SPV中是否包含调试信息和中文字符 ---
def verify_spv(spv_path: Path):
    """使用 spirv-dis (若存在) 反汇编SPV，检查调试信息和中文字符。"""
    dis = shutil.which("spirv-dis")
    if not dis:
        return # 如果找不到 spirv-dis，则跳过验证
    try:
        # 反汇编SPIR-V
        out = subprocess.check_output([dis, str(spv_path)], text=True, encoding="utf-8", errors="replace")
        
        # 检查是否存在调试信息指令
        ok_dbg = ("OpSource" in out) or ("DebugInfo.100" in out)
        # 检查输出中是否存在任何中文字符
        has_cjk = any('\u4e00' <= char <= '\u9fff' for char in out)
        
        if ok_dbg:
            if has_cjk:
                print(f"[VERIFY] {spv_path.name}: 调试信息 ✓, 中文注释 ✓")
            else:
                print(f"[VERIFY] {spv_path.name}: 调试信息 ✓ (未检测到中文注释)")
        else:
            print(f"[VERIFY] {spv_path.name}: 警告: 缺少调试信息! (编译时是否使用了 -gVS/-g?)", file=sys.stderr)
            
    except Exception as e:
        print(f"[VERIFY] {spv_path.name}: 验证失败 - {e}", file=sys.stderr)

def main():
    ap = argparse.ArgumentParser(description="Compile GLSL shaders to SPIR-V, preserving file structure and comments.")
    ap.add_argument("--src", default="glsl", help="Source root directory (default: ./glsl)")
    ap.add_argument("--out", default="spv", help="Output root directory (default: ./spv)")
    ap.add_argument("--target-env", dest="target_env", default="vulkan1.3",
                    help="Target environment, e.g., vulkan1.3 (default: vulkan1.3)")
    ap.add_argument("--config", choices=["debug", "release"], default="debug",
                    help="Build configuration (default: debug)")
    ap.add_argument("--embed-source", dest="embed_source", action="store_true", help="Embed source-level debug info (default: enabled)")
    ap.add_argument("--no-embed-source", dest="embed_source", action="store_false", help="Disable source embedding")
    ap.add_argument("--keep-opt", action="store_true", default=False, help="Keep optimizations in debug build (skip -Od/-O0)")
    ap.add_argument("--debug-symbols-in-release", action="store_true", default=False, help="Also embed debug info in release builds")
    ap.add_argument("--force-utf8-bom", dest="force_utf8_bom", action="store_true", help="Compile from a UTF-8 with BOM temp copy to fix encoding issues (default: enabled)")
    ap.add_argument("--no-force-utf8-bom", dest="force_utf8_bom", action="store_false", help="Disable the UTF-8 BOM temp copy mechanism")
    ap.add_argument("--verify", dest="verify", action="store_true", help="Verify SPV output with spirv-dis (default: enabled)")
    ap.add_argument("--no-verify", dest="verify", action="store_false", help="Disable SPV verification")
    
    # --- 修改：设置参数默认值 ---
    ap.set_defaults(embed_source=True, force_utf8_bom=True, verify=True)
    args = ap.parse_args()

    src_root = Path(args.src).resolve()
    out_root = Path(args.out).resolve()
    out_root.mkdir(parents=True, exist_ok=True)

    kind, tool = find_tool()

    # 收集所有着色器文件
    files = []
    for ext in STAGE_MAP.keys():
        files.extend(src_root.rglob(f"*{ext}"))
    if not files:
        print(f"[INFO] No GLSL files found under {src_root}")
        return

    print(f"[INFO] Found {len(files)} GLSL files. Tool: {kind} ({tool}), Config: {args.config}, Target: {args.target_env}")
    print(f"[INFO] EmbedSource: {args.embed_source}, ForceUTF8BOM: {args.force_utf8_bom}, Verify: {args.verify}")
    
    # --- 修改：使用临时目录上下文管理器，确保脚本结束时自动清理 ---
    with tempfile.TemporaryDirectory(prefix="shader_utf8_bom_") as temp_dir:
        utf8_cache_root = Path(temp_dir) if args.force_utf8_bom else None

        for f in files:
            stage = STAGE_MAP.get(f.suffix.lower())
            if not stage:
                continue

            rel_path = f.relative_to(src_root)

            # --- 关键修改：判断是否需要创建带BOM的UTF-8临时副本 ---
            src_for_compile = f
            if utf8_cache_root:
                src_for_compile = to_utf8_with_bom(f, utf8_cache_root, rel_path)

            # 构建输出路径
            dst_dir = out_root / rel_path.parent
            dst_dir.mkdir(parents=True, exist_ok=True)
            dst = dst_dir / (f.name + ".spv")

            print(f"[BUILD] {rel_path} -> {dst.relative_to(Path.cwd())} (stage={stage})")
            
            # 生成并执行编译命令
            cmd = build_cmd(kind, tool, stage, src_for_compile, dst, args)
            try:
                run(cmd)
                # 编译成功后进行验证
                if args.verify:
                    verify_spv(dst)
            except SystemExit:
                print(f"[FAIL] Failed to compile {rel_path}", file=sys.stderr)
                # 在这里可以选择是继续还是终止，目前是遇到错误就终止
                sys.exit(1)

    print(f"\n[OK] All shaders compiled successfully. Output is in: {out_root}")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\n[FATAL] An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)
    # 移除 input，让脚本可以更好地用于自动化流程
    input("Press Enter to exit...")