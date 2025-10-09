#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
递归检测并转换 .h/.hpp/.cpp 文件编码。
默认将非 UTF-8 with BOM 的文件转换为 UTF-8 with BOM（utf-8-sig）。

用法示例：
  # 1) 默认：当前目录递归扫描并转换为 UTF-8 with BOM
  python reencode_cpp_files.py

  # 2) 指定目录，仅浏览编码（不修改）
  python reencode_cpp_files.py --path D:\proj --dry-run

  # 3) 指定目标编码为 utf-8（无 BOM）
  python reencode_cpp_files.py --target-encoding utf-8

  # 4) 若自动检测失败，假定源编码为 gb18030 再试（谨慎使用）
  python reencode_cpp_files.py --assume-encoding gb18030

  # 5) 生成 .bak 备份
  python reencode_cpp_files.py --backup
"""

import argparse
import os
import sys
import tempfile
import shutil

# ---- 扩展名过滤（可按需增删）----
DEFAULT_EXTS = {".h", ".hpp", ".cpp"}

# ---- 无外部依赖的简单编码检测 ----
# 规则：
#   1) 检查 BOM：UTF-8/UTF-16/UTF-32
#   2) 无 BOM 时按候选编码顺序尝试解码；能严格解码即视为该编码
BOM_MAP = [
    (b"\xEF\xBB\xBF", "utf-8-sig"),           # UTF-8 with BOM
    (b"\xFF\xFE\x00\x00", "utf-32-le"),
    (b"\x00\x00\xFE\xFF", "utf-32-be"),
    (b"\xFF\xFE", "utf-16-le"),
    (b"\xFE\xFF", "utf-16-be"),
]
CANDIDATES = [
    "utf-8",       # 无 BOM UTF-8
    "gb18030",     # 覆盖 gb2312/gbk 常见中文
    "gbk",
    "big5",
    "shift_jis",
    "cp932",       # Windows 日文
]

# 一些目标编码的别名/友好写法
TARGET_ALIASES = {
    "utf8bom": "utf-8-sig",
    "utf-8-bom": "utf-8-sig",
    "utf8-bom": "utf-8-sig",
    "utf8": "utf-8",
}


def is_target_file(path: str, exts=DEFAULT_EXTS) -> bool:
    _, ext = os.path.splitext(path)
    return ext.lower() in exts


def normalize_target_encoding(enc: str) -> str:
    if not enc:
        return "utf-8-sig"
    enc_l = enc.strip().lower()
    return TARGET_ALIASES.get(enc_l, enc_l)


def detect_encoding(raw: bytes):
    """
    返回 (encoding, has_bom_flag)
    若无法确定编码且无法解码，则返回 (None, False)
    """
    # 1) BOM 检测
    for sig, enc in BOM_MAP:
        if raw.startswith(sig):
            return enc, True

    # 2) 候选编码尝试解码（严格模式）
    for enc in CANDIDATES:
        try:
            raw.decode(enc, errors="strict")
            return enc, False
        except Exception:
            continue

    return None, False


def read_binary(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def write_text_atomic(path: str, text: str, encoding: str, make_backup: bool):
    """
    安全写入：先写临时文件，再 os.replace 到原路径。
    可选生成 .bak 备份。
    """
    dirn = os.path.dirname(path) or "."
    fd, tmp_path = tempfile.mkstemp(prefix="reenc_", dir=dirn, text=False)
    os.close(fd)
    try:
        # 保留现有换行符：我们从原始 bytes 解码得到的字符串中仍包含 '\r\n' 或 '\n'，
        # 直接写出且指定 newline='' 可避免平台自动转换。
        with open(tmp_path, "w", encoding=encoding, newline="") as wf:
            wf.write(text)
        if make_backup and os.path.exists(path):
            bak = path + ".bak"
            if not os.path.exists(bak):
                shutil.copy2(path, bak)
        os.replace(tmp_path, path)
    except Exception:
        # 出错时清理临时文件
        try:
            os.remove(tmp_path)
        except Exception:
            pass
        raise


def convert_file(path: str, target_encoding: str, assume_encoding: str = None, dry_run: bool = False, backup: bool = False, verbose: bool = True):
    """
    转换单个文件到目标编码。返回 (action, src_enc, note)
    action in {"skip","convert","error"}
    note: 额外说明
    """
    try:
        raw = read_binary(path)
    except Exception as e:
        return "error", None, f"读取失败: {e}"

    src_enc, has_bom = detect_encoding(raw)

    # 若自动检测失败，尝试使用 --assume-encoding
    if src_enc is None and assume_encoding:
        try:
            raw.decode(assume_encoding, errors="strict")
            src_enc = assume_encoding
        except Exception:
            return "error", None, f"无法识别编码，且按假定编码({assume_encoding})仍解码失败"

    if src_enc is None:
        return "error", None, "无法识别编码（未转换，为避免乱码）"

    # 统一判断：当前是否已是目标编码
    # 注意：utf-8-sig 必须写入 BOM；utf-8 则不带 BOM
    # 对于 utf-16/32，我们也允许转到目标编码
    already_target = False
    if target_encoding == "utf-8-sig":
        # 只有当当前确认为 utf-8 且有 BOM 才算匹配
        already_target = (src_enc == "utf-8-sig")
    else:
        # 其他编码直接比较名称（对 utf-8 不关心 BOM，明确无 BOM 情况）
        already_target = (src_enc.lower() == target_encoding.lower())

        # 特判：如果当前是 utf-8（无 BOM），而目标 enc 是 utf-8（无 BOM）
        if target_encoding == "utf-8" and src_enc in ("utf-8",):
            already_target = True

    if dry_run:
        # 浏览模式：仅显示，不修改
        note = "已是目标编码" if already_target else "将被转换"  # 仅提示，将不会实际转换
        return "skip", src_enc, note

    if already_target:
        return "skip", src_enc, "无需转换"

    # 真正转换
    try:
        text = raw.decode(src_enc, errors="strict")
    except Exception as e:
        return "error", src_enc, f"源解码失败: {e}"

    try:
        write_text_atomic(path, text, target_encoding, make_backup=backup)
        return "convert", src_enc, f"已转换为 {target_encoding}"
    except Exception as e:
        return "error", src_enc, f"写入失败: {e}"


def pause_for_keypress():
    # Windows：任意键；类 Unix：回车
    print("\n完成。按任意键退出...", flush=True)
    try:
        import msvcrt  # type: ignore
        msvcrt.getch()
    except Exception:
        try:
            input()
        except EOFError:
            pass


def main():
    parser = argparse.ArgumentParser(description="递归检测并转换 .h/.hpp/.cpp 文件编码。默认转为 UTF-8 with BOM（utf-8-sig）。")
    parser.add_argument("--path", type=str, default=".", help="起始目录（默认当前目录）")
    parser.add_argument("--target-encoding", type=str, default="utf-8-sig", help="目标编码（默认 utf-8-sig = UTF-8 with BOM）")
    parser.add_argument("--assume-encoding", type=str, default=None, help="当自动检测失败时，假定的源编码（谨慎使用）。")
    parser.add_argument("--dry-run", action="store_true", help="仅浏览编码信息，不做修改。")
    parser.add_argument("--backup", action="store_true", help="转换前生成 .bak 备份。")
    parser.add_argument("--no-pause", action="store_true", help="结束后不等待按键退出。")
    parser.add_argument("--exts", type=str, default=".h,.hpp,.cpp", help="要处理的扩展名，逗号分隔（默认 .h,.hpp,.cpp）")
    args = parser.parse_args()

    start_dir = os.path.abspath(args.path)
    target_encoding = normalize_target_encoding(args.target_encoding)
    assume_encoding = args.assume_encoding
    dry_run = args.dry_run
    backup = args.backup
    exts = {e.strip().lower() if e.strip().startswith(".") else "." + e.strip().lower()
            for e in args.exts.split(",") if e.strip()}

    if not os.path.isdir(start_dir):
        print(f"[错误] 目录不存在：{start_dir}")
        if not args.no_pause:
            pause_for_keypress()
        sys.exit(1)

    print(f"起始目录：{start_dir}")
    print(f"目标编码：{target_encoding}  {'(仅浏览，不修改)' if dry_run else ''}")
    if assume_encoding:
        print(f"假定源编码（备选）：{assume_encoding}")
    print(f"处理扩展名：{', '.join(sorted(exts))}")
    if backup and not dry_run:
        print("转换前将生成 .bak 备份文件")
    print("-" * 80)

    total_files = 0
    converted = 0
    skipped = 0
    errors = 0

    for root, dirs, files in os.walk(start_dir):
        # 可选：跳过隐藏目录（如 .git）
        # dirs[:] = [d for d in dirs if not d.startswith(".")]
        for name in files:
            path = os.path.join(root, name)
            if not is_target_file(path, exts):
                continue

            total_files += 1
            action, src_enc, note = convert_file(
                path,
                target_encoding=target_encoding,
                assume_encoding=assume_encoding,
                dry_run=dry_run,
                backup=backup,
                verbose=True
            )

            src_enc_disp = src_enc or "未知编码"
            rel = os.path.relpath(path, start_dir)
            if action == "convert":
                converted += 1
                print(f"[转换] {rel} | 源编码: {src_enc_disp} -> {note}")
            elif action == "skip":
                skipped += 1
                print(f"[跳过] {rel} | 源编码: {src_enc_disp} | {note}")
            else:
                errors += 1
                print(f"[错误] {rel} | 源编码: {src_enc_disp} | {note}")

    print("-" * 80)
    print(f"文件总数: {total_files}  | 转换: {converted}  | 跳过: {skipped}  | 错误: {errors}")

    if not args.no_pause:
        pause_for_keypress()


if __name__ == "__main__":
    main()
