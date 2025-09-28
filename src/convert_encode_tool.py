#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
批量预览 / 转换文件编码（默认目标：UTF-8 with BOM, 即 utf-8-sig）
- 预览：列出编码、BOM、检测置信度、是否会被转换等
- 转换：将文本文件写回指定编码；可备份 .bak
"""

from __future__ import annotations
import argparse
import sys
import os
from pathlib import Path
from typing import Iterable, Optional, Tuple, List, Dict
import csv

# ------------------------------
# 检测器加载：优先 charset-normalizer，其次 chardet
# ------------------------------
DETECTOR_KIND = None
CN_from_path = None
CHARDET_mod = None

def _load_detectors():
    global DETECTOR_KIND, CN_from_path, CHARDET_mod
    try:
        from charset_normalizer import from_path as _cn_from_path
        DETECTOR_KIND = "charset-normalizer"
        CN_from_path = _cn_from_path
        return
    except Exception:
        pass
    try:
        import chardet as _chardet
        DETECTOR_KIND = "chardet"
        CHARDET_mod = _chardet
        return
    except Exception:
        pass
    DETECTOR_KIND = None

_load_detectors()

# ------------------------------
# 工具函数
# ------------------------------
PRINTABLE_BYTES = set(range(32, 127)) | {9, 10, 13}  # tab, lf, cr

def is_probably_binary(p: Path, probe_size: int = 4096, nonprint_threshold: float = 0.30) -> bool:
    """
    通过样本启发判断是否为二进制：
    - 出现 NUL 字节
    - 非可打印字节比例过高
    """
    try:
        with p.open('rb') as f:
            chunk = f.read(probe_size)
    except Exception:
        return True
    if b'\x00' in chunk:
        return True
    if not chunk:
        return False
    nonprint = sum(b not in PRINTABLE_BYTES for b in chunk)
    return (nonprint / len(chunk)) > nonprint_threshold

def sniff_bom(p: Path) -> Tuple[bool, Optional[str]]:
    """
    返回 (has_bom, bom_type)
    bom_type in {"utf-8-sig","utf-16-le","utf-16-be","utf-32-le","utf-32-be", None}
    """
    try:
        with p.open('rb') as f:
            head = f.read(4)
    except Exception:
        return (False, None)

    if head.startswith(b'\xef\xbb\xbf'):
        return True, "utf-8-sig"
    if head.startswith(b'\xff\xfe\x00\x00'):
        return True, "utf-32-le"
    if head.startswith(b'\x00\x00\xfe\xff'):
        return True, "utf-32-be"
    if head.startswith(b'\xff\xfe'):
        return True, "utf-16-le"
    if head.startswith(b'\xfe\xff'):
        return True, "utf-16-be"
    return False, None

def detect_encoding(p: Path) -> Tuple[Optional[str], float]:
    """
    返回 (encoding, confidence[0..1])，若无法检测，返回 (None, 0.0)
    """
    if DETECTOR_KIND == "charset-normalizer":
        try:
            result = CN_from_path(str(p)).best()
            if result:
                enc = result.encoding
                # 经验映射：越少乱码、越高语义一致性 -> 置信度越高
                conf = 0.65 * (1.0 - result.chaos) + 0.35 * result.coherence
                # 若存在并匹配到合理的 BOM，略增信
                if result.bom and enc.lower().startswith(("utf-8", "utf_8", "utf-16", "utf_32")):
                    conf += 0.05
                conf = max(0.0, min(1.0, conf))
                return (enc, conf)
        except Exception:
            return (None, 0.0)
    elif DETECTOR_KIND == "chardet":
        try:
            with p.open('rb') as f:
                raw = f.read()
            det = CHARDET_mod.detect(raw)
            enc = det.get('encoding')
            conf = float(det.get('confidence') or 0.0)
            # chardet 的 conf 也是 0..1
            return (enc, conf)
        except Exception:
            return (None, 0.0)
    else:
        # 没有检测器
        return (None, 0.0)

def normalize_target_encoding(s: str) -> str:
    """
    归一化目标编码命名；支持一些常见别名
    - 'utf-8-sig' == 'utf-8 with bom' == 'utf8-bom' == 'utf-8-with-bom'
    """
    key = s.strip().lower().replace('_', '-').replace(' ', '')
    aliases = {
        'utf-8-sig': 'utf-8-sig',
        'utf8-sig': 'utf-8-sig',
        'utf-8withbom': 'utf-8-sig',
        'utf8-with-bom': 'utf-8-sig',
        'utf8bom': 'utf-8-sig',
        'utf-8-bom': 'utf-8-sig',
        'utf8': 'utf-8',
        'utf-8': 'utf-8',
        'gbk': 'gbk',
        'gb18030': 'gb18030',
        'cp936': 'cp936',
        'shift-jis': 'shift_jis',
        'shiftjis': 'shift_jis',
        'sjis': 'shift_jis',
        'cp932': 'cp932',
    }
    return aliases.get(key, s)

def iter_files(root: Path, recursive: bool, exts: List[str] | None, include_hidden: bool) -> Iterable[Path]:
    def want(p: Path) -> bool:
        if not include_hidden:
            parts = p.parts
            if any(name.startswith('.') for name in parts):
                return False
        if exts:
            return p.suffix.lower() in exts
        return True

    if recursive:
        for p in root.rglob('*'):
            if p.is_file() and want(p):
                yield p
    else:
        for p in root.glob('*'):
            if p.is_file() and want(p):
                yield p

def will_convert(enc_now: Optional[str], has_bom_now: bool, target_enc: str) -> bool:
    """
    判断是否需要转换：
    - 若目标是 utf-8-sig：无论现在是否 utf-8，只要没 BOM 就需要写回；或非 utf-8 也需要
    - 若目标是 utf-8（无 BOM）：不是 utf-8 或者现在有 BOM 就需要
    - 其他编码：只要 enc_now != target_enc 就需要
    """
    if target_enc == 'utf-8-sig':
        if enc_now is None:
            return True
        low = enc_now.lower().replace('_', '-')
        if low.startswith('utf-8'):
            return not has_bom_now  # utf-8 但没 BOM 要加
        return True
    elif target_enc == 'utf-8':
        if enc_now is None:
            return True
        low = enc_now.lower().replace('_', '-')
        if low.startswith('utf-8'):
            return has_bom_now  # 去掉 BOM
        return True
    else:
        return (enc_now or '').lower().replace('_', '-') != target_enc.lower().replace('_', '-')

def write_report(rows: List[Dict], csv_path: Path):
    header = ["path", "size", "encoding", "confidence", "has_bom", "bom_type", "binary", "need_convert", "reason"]
    with csv_path.open('w', newline='', encoding='utf-8-sig') as f:
        w = csv.DictWriter(f, fieldnames=header)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k) for k in header})

def human_size(n: int) -> str:
    for unit in ['B','KB','MB','GB','TB']:
        if n < 1024.0:
            return f"{n:.1f}{unit}"
        n /= 1024.0
    return f"{n:.1f}PB"

# ------------------------------
# 预览
# ------------------------------
def cmd_preview(args: argparse.Namespace) -> int:
    root = Path(args.root).resolve()
    if not root.exists() or not root.is_dir():
        print(f"[ERROR] 目录不存在：{root}")
        return 2

    if DETECTOR_KIND is None:
        print("[WARN] 未找到编码检测库。请先安装之一：\n  pip install charset-normalizer\n  或：pip install chardet\n")

    exts = [e.lower() if e.startswith('.') else f".{e.lower()}" for e in (args.ext or [])]
    rows = []
    count = 0

    for p in iter_files(root, args.recursive, exts or None, args.include_hidden):
        count += 1
        size = p.stat().st_size
        isbin = is_probably_binary(p)
        has_bom, bom_type = sniff_bom(p)
        enc, conf = (None, 0.0) if isbin else detect_encoding(p)
        need = (not isbin) and will_convert(enc, has_bom, normalize_target_encoding(args.to))
        reason = ""
        if isbin:
            reason = "binary-skip"
        elif enc is None:
            reason = "unknown-encoding"
        elif need:
            reason = "will-convert"
        else:
            reason = "no-change"

        rows.append({
            "path": str(p),
            "size": size,
            "encoding": enc or "",
            "confidence": f"{conf:.2f}",
            "has_bom": has_bom,
            "bom_type": bom_type or "",
            "binary": isbin,
            "need_convert": need,
            "reason": reason
        })

    # 控制台输出（简表）
    print(f"\n[预览] 根目录：{root}")
    print(f"[预览] 检测器：{DETECTOR_KIND or '无（将无法检测编码）'}")
    print(f"[预览] 文件数：{count}\n")
    print(f"{'Path':70}  {'Size':>8}  {'Enc':>12}  {'Conf':>5}  {'BOM':>3}  {'Need':>5}")
    print("-"*108)
    for r in rows[:args.max_print]:
        p = r["path"]
        print(f"{(p[:67]+'...' if len(p)>70 else p):70}  "
              f"{human_size(r['size']):>8}  "
              f"{(r['encoding'] or '-'):>12}  "
              f"{r['confidence']:>5}  "
              f"{('Y' if r['has_bom'] else 'N'):>3}  "
              f"{('Y' if r['need_convert'] else 'N'):>5}")
    if len(rows) > args.max_print:
        print(f"... 共 {len(rows)} 条，已截断显示。")

    if args.report:
        report_path = Path(args.report).resolve()
        write_report(rows, report_path)
        print(f"\n[报告] 已写出：{report_path}")

    # 总结
    will = sum(1 for r in rows if r["need_convert"])
    print(f"\n[统计] 预计需要转换：{will} / {len(rows)}（已自动跳过二进制与未知编码文件）")

    return 0

# ------------------------------
# 转换
# ------------------------------
def convert_one_file(p: Path, target_enc: str, errors: str, force_unknown: bool, make_backup: bool,
                     min_conf: float) -> Tuple[bool, str]:
    """
    执行单文件转换；返回 (changed, message)
    """
    size = p.stat().st_size
    isbin = is_probably_binary(p)
    if isbin:
        return (False, "skip: binary")

    enc, conf = detect_encoding(p)
    has_bom, bom_type = sniff_bom(p)

    if enc is None and not force_unknown:
        return (False, "skip: unknown-encoding (use --force-unknown to override)")

    if not will_convert(enc, has_bom, target_enc):
        return (False, "skip: no-change")

    # 低置信度保护
    if enc is not None and conf < min_conf and not force_unknown:
        return (False, f"skip: low-confidence {conf:.2f} (<{min_conf:.2f})")

    # 读取并转码
    try:
        with p.open('rb') as f:
            raw = f.read()
    except Exception as e:
        return (False, f"error: read fail ({e})")

    src_enc = enc or 'utf-8'  # 兜底：未知时默认按 utf-8 解（若失败会抛错）
    try:
        text = raw.decode(src_enc, errors=errors)
    except Exception as e:
        return (False, f"error: decode with {src_enc} failed ({e})")

    # 备份
    if make_backup:
        try:
            bak = p.with_suffix(p.suffix + ".bak")
            if not bak.exists():
                with bak.open('wb') as f:
                    f.write(raw)
        except Exception as e:
            return (False, f"error: backup failed ({e})")

    # 写回
    try:
        # utf-8-sig 会自动写入 BOM；utf-8 不会
        with p.open('w', encoding=target_enc, newline='') as f:
            f.write(text)
    except Exception as e:
        return (False, f"error: write failed ({e})")

    return (True, f"ok: {enc or 'unknown'} -> {target_enc}")

def cmd_convert(args: argparse.Namespace) -> int:
    root = Path(args.root).resolve()
    if not root.exists() or not root.is_dir():
        print(f"[ERROR] 目录不存在：{root}")
        return 2

    if DETECTOR_KIND is None and not args.force_unknown:
        print("[WARN] 未找到编码检测库。建议先安装：pip install charset-normalizer 或 pip install chardet")
        print("      否则请加 --force-unknown（将按 utf-8 解读未知编码，可能产生乱码）。\n")

    exts = [e.lower() if e.startswith('.') else f".{e.lower()}" for e in (args.ext or [])]
    target_enc = normalize_target_encoding(args.to)
    changed, skipped, failed = 0, 0, 0

    files = list(iter_files(root, args.recursive, exts or None, args.include_hidden))
    print(f"[转换] 根目录：{root}")
    print(f"[转换] 目标编码：{target_enc}")
    print(f"[转换] 待检查文件数：{len(files)}\n")

    for p in files:
        ok, msg = convert_one_file(
            p, target_enc=target_enc, errors=args.errors,
            force_unknown=args.force_unknown, make_backup=args.backup,
            min_conf=args.min_confidence
        )
        if ok:
            changed += 1
            state = "CHANGED"
        else:
            if msg.startswith("error:"):
                failed += 1
                state = "ERROR "
            else:
                skipped += 1
                state = "SKIP  "
        # 控制台输出
        print(f"{state} | {p} | {msg}")

    print("\n[结果] 修改：{}，跳过：{}，失败：{}".format(changed, skipped, failed))
    return 0 if failed == 0 else 1

# ------------------------------
# CLI
# ------------------------------
def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="预览并批量转换目录下文件编码（默认目标：UTF-8 with BOM / utf-8-sig）"
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("root", help="根目录路径")
    common.add_argument("--recursive", action="store_true", help="递归子目录")
    common.add_argument("--ext", nargs='*', help="只处理这些后缀（例：--ext .txt .md .cpp .h）")
    common.add_argument("--include-hidden", action="store_true", help="包含隐藏文件/目录（默认不包含）")
    common.add_argument("--to", default="utf-8-sig", help="目标编码（默认 utf-8-sig，即 UTF-8 with BOM）")

    p_prev = sub.add_parser("preview", parents=[common], help="仅预览编码，不修改文件")
    p_prev.add_argument("--report", help="将结果写入 CSV 报告文件（UTF-8 with BOM）")
    p_prev.add_argument("--max-print", type=int, default=200, help="控制台最多打印多少条（默认200）")

    p_conv = sub.add_parser("convert", parents=[common], help="执行转换（会修改文件）")
    p_conv.add_argument("--backup", action="store_true", help="写回前生成 .bak 备份")
    p_conv.add_argument("--errors", default="strict", choices=["strict","ignore","replace"],
                        help="解码出错时的策略（默认 strict）")
    p_conv.add_argument("--force-unknown", action="store_true",
                        help="对未知编码也强制转换（按 utf-8 解码，风险自担）")
    p_conv.add_argument("--min-confidence", dest="min_confidence", type=float, default=0.20,
                        help="低于该置信度则跳过（默认0.20）。--force-unknown 可忽略该限制。")

    return p

def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.cmd == "preview":
        return cmd_preview(args)
    elif args.cmd == "convert":
        return cmd_convert(args)
    else:
        parser.print_help()
        return 2

if __name__ == "__main__":
    main()
    input("Press Enter to exit...")
