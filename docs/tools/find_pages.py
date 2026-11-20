#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在导出的 PDF 中定位报告标题所在页，回填目录页码（两遍法第二遍）。

用法：
  python find_pages.py 课程设计报告.pdf toc_entries.json toc_pages.json

逻辑：
  1. 正文每页页眉都有“课程设计报告”，据此确定正文起始页；
  2. 按目录顺序单调向后扫描，找到每个标题首次出现的正文页；
  3. 输出 {标题: 正文页码} 供 make_report.py --pages 使用。
"""

import json
import re
import sys

try:  # 新版 PyMuPDF 推荐 import pymupdf；旧版只有 fitz
    import pymupdf as fitz
except ImportError:  # pragma: no cover
    import fitz


def norm(s):
    return re.sub(r"\s+", "", s)


def main(argv):
    if len(argv) < 4:
        print("usage: find_pages.py report.pdf toc_entries.json out.json")
        return 2
    pdf_path, entries_path, out_path = argv[1], argv[2], argv[3]
    entries = json.load(open(entries_path, encoding="utf-8"))
    doc = fitz.open(pdf_path)

    pages = []
    for page in doc:
        text = page.get_text("text")
        pages.append(text)

    header = norm("课程设计报告")
    body_start = None
    for i, text in enumerate(pages):
        # 正文页页脚形如“第 1 页”，用它定位正文起始页（避免与封面标题混淆）
        if re.search(r"第\s*1\s*页", text) and header in norm(text):
            body_start = i
            break
    if body_start is None:
        for i, text in enumerate(pages):
            first_line = next((ln for ln in text.splitlines() if ln.strip()), "")
            if norm(first_line) == header:
                body_start = i
                break
    if body_start is None:
        print("warning: body start not found, assume 0")
        body_start = 0

    result = {}
    cursor = body_start
    for entry in entries:
        if entry.strip() == "摘  要" or entry.strip() == "摘要":
            result[entry] = "I"  # 摘要位于前置部分，用罗马数字
            continue
        key = norm(entry)
        found = None
        for i in range(cursor, len(pages)):
            if key in norm(pages[i]):
                found = i
                break
        if found is None:
            result[entry] = "00"
        else:
            result[entry] = found - body_start + 1
            cursor = found

    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print(f"pages={len(pages)} body_start={body_start + 1}")
    for entry in entries:
        print(f"  {result[entry]!s:>4}  {entry}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
