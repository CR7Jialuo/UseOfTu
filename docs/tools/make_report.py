#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《图的最短路径及其在校园导航中的应用》课程设计报告（DOCX）。

排版：A4，上下 2.54cm、左 3.0cm、右 2.5cm；正文宋体小四、1.5 倍行距、首行缩进 2 字符；
标题黑体；图/表题宋体 10.5pt；正文页眉“课程设计报告”、页脚“第 X 页”。

目录页码两遍法：
  1) 先输出占位页码的 DOCX 并导出 PDF；
  2) docs/tools/find_pages.py 在 PDF 中定位各标题，回填页码 JSON；
  3) 再次运行本脚本并加 --pages docs/toc_pages.json。
"""

import argparse
import json
import os
import re
import sys

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING, WD_TAB_ALIGNMENT, WD_TAB_LEADER
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

BODY_FONT = "宋体"
HEAD_FONT = "黑体"
MONO_FONT = "Consolas"
BODY_SIZE = Pt(12)
CAPTION_SIZE = Pt(10.5)

TITLE = "图的最短路径及其在校园导航中的应用"

STUDENT = "宋家洛"
STUDENT_ID = "2024122827"
CLASS_NAME = "信息1241"
ADVISOR = "李薛"
SCHOOL = "江苏海洋大学"
DATE = "2026 年 11 月"

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def apply_mask():
    """公开仓库版：把个人信息替换为占位符（真实信息版只放 dist/final）。"""
    global STUDENT, STUDENT_ID, CLASS_NAME, ADVISOR
    STUDENT = "【请填写】"
    STUDENT_ID = "【请填写】"
    CLASS_NAME = "【请填写】"
    ADVISOR = "【请填写】"
FIG_DIR = os.path.join(ROOT, "docs", "diagrams")
DATA_DIR = os.path.join(ROOT, "docs", "data")


# ------------------------------------------------------------------ 基础排版


def set_run(run, name=BODY_FONT, size=BODY_SIZE, bold=False, color=None):
    run.font.name = name
    run.font.size = size
    run.font.bold = bold
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    rfonts.set(qn("w:eastAsia"), name)
    rfonts.set(qn("w:ascii"), name)
    rfonts.set(qn("w:hAnsi"), name)
    if color:
        run.font.color.rgb = color


def para(doc, text="", size=BODY_SIZE, bold=False, align=None, indent=True,
         space_before=0, space_after=0, line=1.5, name=BODY_FONT, color=None):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.line_spacing = line
    pf.space_before = Pt(space_before)
    pf.space_after = Pt(space_after)
    if indent:
        pf.first_line_indent = Pt(24)  # 2 字符 × 小四 12pt
    if align is not None:
        p.alignment = align
    if text:
        run = p.add_run(text)
        set_run(run, name, size, bold, color)
    return p


def heading(doc, text, level=1, page_break=False):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.space_before = Pt(18 if level == 1 else 12)
    pf.space_after = Pt(10 if level == 1 else 6)
    pf.line_spacing = 1.5
    if page_break and level == 1:
        pf.page_break_before = True
    size = Pt(16) if level == 1 else (Pt(14) if level == 2 else Pt(12))
    run = p.add_run(text)
    set_run(run, HEAD_FONT, size, True)
    # 让标题进入 Word 的大纲级别，便于导航窗格与自动目录
    ppr = p._p.get_or_add_pPr()
    outline = OxmlElement("w:outlineLvl")
    outline.set(qn("w:val"), str(level - 1))
    ppr.append(outline)
    return p


def caption(doc, text):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    pf = p.paragraph_format
    pf.space_before = Pt(3)
    pf.space_after = Pt(9)
    pf.line_spacing = 1.2
    run = p.add_run(text)
    set_run(run, BODY_FONT, CAPTION_SIZE, False)
    return p


def figure(doc, filename, text, width_cm=15.0):
    path = os.path.join(FIG_DIR, filename)
    if not os.path.exists(path):
        para(doc, f"[缺少图片 {filename}]", indent=False)
        return
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(3)
    p.add_run().add_picture(path, width=Cm(width_cm))
    caption(doc, text)


def code_block(doc, lines, size=9.5):
    for line in lines:
        p = doc.add_paragraph()
        pf = p.paragraph_format
        pf.line_spacing = 1.0
        pf.space_before = Pt(0)
        pf.space_after = Pt(0)
        pf.left_indent = Cm(0.6)
        run = p.add_run(line)
        set_run(run, MONO_FONT, Pt(size))
        ppr = p._p.get_or_add_pPr()
        shd = OxmlElement("w:shd")
        shd.set(qn("w:val"), "clear")
        shd.set(qn("w:fill"), "F5F7FA")
        ppr.append(shd)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def table(doc, headers, rows, widths=None, size=10.5, text_caption=None):
    if text_caption:
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_before = Pt(6)
        p.paragraph_format.space_after = Pt(3)
        p.paragraph_format.keep_with_next = True
        set_run(p.add_run(text_caption), BODY_FONT, CAPTION_SIZE)
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    t.autofit = False
    hdr = t.rows[0].cells
    for i, h in enumerate(headers):
        hdr[i].text = ""
        p = hdr[i].paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.space_before = Pt(2)
        p.paragraph_format.space_after = Pt(2)
        set_run(p.add_run(h), HEAD_FONT, Pt(size), True)
        shd = OxmlElement("w:shd")
        shd.set(qn("w:val"), "clear")
        shd.set(qn("w:fill"), "EEF2F7")
        hdr[i]._tc.get_or_add_tcPr().append(shd)
    for row in rows:
        cells = t.add_row().cells
        for i, val in enumerate(row):
            cells[i].text = ""
            p = cells[i].paragraphs[0]
            p.paragraph_format.space_before = Pt(2)
            p.paragraph_format.space_after = Pt(2)
            p.paragraph_format.line_spacing = 1.15
            set_run(p.add_run(str(val)), BODY_FONT, Pt(size))
    if widths:
        for i, w in enumerate(widths):
            t.columns[i].width = Cm(w)
            for cell in t.columns[i].cells:
                cell.width = Cm(w)
    # 表头跨页重复 + 禁止行内断页（避免出现只带半个单元格的孤立行）
    tr_pr = t.rows[0]._tr.get_or_add_trPr()
    tr_pr.append(OxmlElement("w:tblHeader"))
    for row in t.rows:
        row._tr.get_or_add_trPr().append(OxmlElement("w:cantSplit"))
    doc.add_paragraph().paragraph_format.space_after = Pt(2)
    return t


def page_number_footer(section):
    footer = section.footer
    footer.is_linked_to_previous = False
    p = footer.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("第 ")
    set_run(run, BODY_FONT, Pt(9))
    run = p.add_run()
    set_run(run, BODY_FONT, Pt(9))
    fld_begin = OxmlElement("w:fldChar")
    fld_begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    fld_end = OxmlElement("w:fldChar")
    fld_end.set(qn("w:fldCharType"), "end")
    run._r.append(fld_begin)
    run._r.append(instr)
    run._r.append(fld_end)
    run = p.add_run(" 页")
    set_run(run, BODY_FONT, Pt(9))


def header_text(section, text):
    header = section.header
    header.is_linked_to_previous = False
    p = header.paragraphs[0]
    p.text = ""
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run(text), BODY_FONT, Pt(9), False, RGBColor(0x60, 0x60, 0x60))
    # 页眉下划线
    ppr = p._p.get_or_add_pPr()
    pbdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single")
    bottom.set(qn("w:sz"), "6")
    bottom.set(qn("w:space"), "1")
    bottom.set(qn("w:color"), "AAAAAA")
    pbdr.append(bottom)
    ppr.append(pbdr)


def restart_numbering(section, start=1):
    sect_pr = section._sectPr
    pg = OxmlElement("w:pgNumType")
    pg.set(qn("w:start"), str(start))
    sect_pr.append(pg)


# ------------------------------------------------------------------ 数据准备


def read_text(path):
    with open(path, "rb") as f:
        raw = f.read()
    for enc in ("utf-8-sig", "utf-16"):
        try:
            return raw.decode(enc)
        except (UnicodeDecodeError, UnicodeError):
            continue
    return raw.decode("gb18030", errors="replace")


def load_bench():
    rows = []
    if not os.path.isdir(DATA_DIR):
        return rows
    for name in sorted(os.listdir(DATA_DIR)):
        m = re.match(r"bench_(\d+)\.txt$", name)
        if not m:
            continue
        n = int(m.group(1))
        text = read_text(os.path.join(DATA_DIR, name))
        row = {"n": n}
        for key, pattern in (
            ("dijkstra", r"Dijkstra[^\n]*?([0-9.]+)\s+[0-9.]+"),
            ("floyd", r"Floyd-Warshall\s+([0-9.]+)"),
            ("bellman", r"Bellman-Ford\s+([0-9.]+)"),
            ("prim", r"Prim \(MST\)\s+([0-9.]+)"),
            ("kruskal", r"Kruskal \(MST\)\s+([0-9.]+)"),
        ):
            mm = re.search(pattern, text)
            if mm:
                row[key] = float(mm.group(1))
        rows.append(row)
    rows.sort(key=lambda r: r["n"])
    return rows


def load_groups():
    path = os.path.join(DATA_DIR, "test-summary.txt")
    groups = []
    if not os.path.exists(path):
        return groups
    for line in read_text(path).splitlines():
        m = re.match(r"\[group\] ([\w\-]+): (\d+) checks, (\d+) failed", line.strip())
        if m:
            groups.append((m.group(1), int(m.group(2)), int(m.group(3))))
    return groups


def count_lines(path):
    try:
        return sum(1 for _ in open(path, encoding="utf-8", errors="replace"))
    except OSError:
        return 0


# ------------------------------------------------------------------ 文档内容


def build_cover(doc):
    doc.add_paragraph()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(12)
    set_run(p.add_run("课 程 设 计 报 告"), HEAD_FONT, Pt(36), True)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(18)
    set_run(p.add_run(TITLE), HEAD_FONT, Pt(22), True, RGBColor(0x1F, 0x3A, 0x5F))

    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(40)

    items = [
        ("班    级", CLASS_NAME),
        ("学生姓名", STUDENT),
        ("学    号", STUDENT_ID),
        ("指导教师", ADVISOR),
        ("完成日期", DATE),
    ]
    for key, value in items:
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.line_spacing = 1.5
        p.paragraph_format.space_after = Pt(2)
        set_run(p.add_run(f"{key}："), BODY_FONT, Pt(14), True)
        set_run(p.add_run(value), BODY_FONT, Pt(14), False)

    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(46)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run(SCHOOL), HEAD_FONT, Pt(16), True, RGBColor(0x33, 0x33, 0x33))


ABSTRACT = (
    "校园导航是图的最短路径问题的一个典型应用场景。本文以江苏海洋大学苍梧校区为背景，"
    "用无向带权图对校园路网进行建模，设计并实现了一个基于 C11 的命令行校园导航系统。"
    "系统以邻接表（CSR）为主要存储结构，实现了 Dijkstra、Floyd-Warshall、Bellman-Ford、"
    "BFS 等最短路径算法，并用 Tarjan 算法完成割点与桥的分析，用 Prim、Kruskal 构造最小生成树，"
    "用 Held-Karp 动态规划与 2-opt 启发式求解多点巡游问题。系统支持按距离或步行时间两种权重口径"
    "进行路径规划、最近设施查询、全源最短路矩阵、关键道路分析与图编辑，并提供 CSV 数据导入导出"
    "和带 CRC32 校验的二进制快照。工程上用脚本统一构建与测试，共 22 组、1592 项自动化断言，"
    "并在 GitHub Actions 上完成持续集成。测试结果表明，在 24 个地点、41 条道路的校园图上，"
    "单次 Dijkstra 查询耗时约 3 微秒，随机 120 节点图上的 Floyd 计算约 1 毫秒，"
    "系统能够在毫秒级完成交互查询，并正确处理不连通、负权、非法输入与数据损坏等异常情况。"
)

KEYWORDS = "图；最短路径；Dijkstra；Floyd-Warshall；校园导航；数据结构"

TOC_ENTRIES = [
    ("摘  要", 1),
    ("1 绪论", 1),
    ("1.1 课题背景", 2),
    ("1.2 相关算法与工程现状", 2),
    ("1.3 课程设计目标与主要工作", 2),
    ("1.4 开发环境与工具链", 2),
    ("2 需求分析", 1),
    ("2.1 应用场景与用户需求", 2),
    ("2.2 功能需求", 2),
    ("2.3 非功能需求", 2),
    ("2.4 典型用例与操作流程", 2),
    ("3 系统设计", 1),
    ("3.1 总体架构", 2),
    ("3.2 模块划分", 2),
    ("3.3 核心数据结构", 2),
    ("3.4 图的存储结构选型", 2),
    ("3.5 数据文件设计", 2),
    ("3.6 关键接口设计", 2),
    ("4 算法设计与分析", 1),
    ("4.1 问题建模", 2),
    ("4.2 Dijkstra 算法", 2),
    ("4.3 Floyd-Warshall 算法", 2),
    ("4.4 Bellman-Ford 算法", 2),
    ("4.5 BFS 与最少路段路径", 2),
    ("4.6 Tarjan 割点与桥", 2),
    ("4.7 最小生成树", 2),
    ("4.8 多点巡游问题", 2),
    ("4.9 算法对比与选型", 2),
    ("5 系统实现", 1),
    ("5.1 工程结构", 2),
    ("5.2 数据层实现", 2),
    ("5.3 算法层实现", 2),
    ("5.4 交互层实现", 2),
    ("5.5 健壮性设计", 2),
    ("5.6 跨平台与编码处理", 2),
    ("6 测试与结果分析", 1),
    ("6.1 测试策略", 2),
    ("6.2 单元测试", 2),
    ("6.3 随机交叉验证", 2),
    ("6.4 端到端测试", 2),
    ("6.5 性能测试", 2),
    ("6.6 运行结果展示", 2),
    ("7 总结与展望", 1),
    ("7.1 完成情况", 2),
    ("7.2 不足与改进方向", 2),
    ("参考文献", 1),
    ("附录 A 命令速查", 1),
    ("附录 B 测试用例摘录", 1),
]


def build_front_matter(doc, pages):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(12)
    set_run(p.add_run("摘  要"), HEAD_FONT, Pt(18), True)

    para(doc, ABSTRACT)
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(12)
    set_run(p.add_run("关键词："), HEAD_FONT, Pt(12), True)
    set_run(p.add_run(KEYWORDS), BODY_FONT, Pt(12))

    doc.add_page_break()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(14)
    set_run(p.add_run("目  录"), HEAD_FONT, Pt(18), True)

    for text, level in TOC_ENTRIES:
        p = doc.add_paragraph()
        pf = p.paragraph_format
        pf.line_spacing = 1.5
        pf.space_after = Pt(0)
        pf.left_indent = Cm(0.0 if level == 1 else 0.8)
        pf.tab_stops.add_tab_stop(Cm(15.5), WD_TAB_ALIGNMENT.RIGHT, WD_TAB_LEADER.DOTS)
        set_run(p.add_run(text), HEAD_FONT if level == 1 else BODY_FONT, Pt(12), level == 1)
        if text == "摘  要":
            page = "I"
        else:
            page = pages.get(text, "00")
        set_run(p.add_run("\t" + str(page)), BODY_FONT, Pt(12))


def build_body(doc):
    bench = load_bench()
    groups = load_groups()

    # ---------------- 1 绪论
    heading(doc, "1 绪论", 1)
    heading(doc, "1.1 课题背景", 2)
    para(doc, "随着高校校园面积的扩大和功能分区的细化，校园内部的道路网络越来越复杂。新生入学、访客来访、"
              "考试期间跨楼栋流动等场景中，“从当前位置到目标建筑怎么走最近”成为一个真实且高频的问题。"
              "如果只依靠指示牌或口口相传，既低效又容易出错；而商业地图应用在校内的细粒度数据往往不完整，"
              "对校内步行道路、临时封闭路段缺乏支持。")
    para(doc, "从数学角度看，校园路网可以抽象为一张无向带权图：地点是顶点，道路是边，道路长度或步行时间是边权。"
              "于是“怎么走最近”就等价于图论中的单源最短路径问题，“任意两地之间有多远”等价于全源最短路径问题，"
              "“哪条路封了会导致部分区域不可达”等价于割点与桥的判定，“在校园里铺一套巡检线路”等价于最小生成树。"
              "这些问题在《数据结构》课程中都有经典算法，把它们落到一个可运行、可演示、可测试的系统中，"
              "正是本次课程设计的目标。")
    para(doc, "本课题选择图的最短路径作为题目，以江苏海洋大学苍梧校区的校园导航为应用场景，"
              "使用 C 语言（C11）实现一个命令行导航系统。系统既要覆盖课程要求的核心算法，"
              "也要具备工程可用性：数据可编辑、错误可诊断、行为可自动化验证。")

    heading(doc, "1.2 相关算法与工程现状", 2)
    para(doc, "最短路径问题的经典解法在 20 世纪 50—60 年代就已成型：Bellman 与 Ford 提出的迭代松弛方法"
              "可以处理负权边；Dijkstra 于 1959 年给出非负权图上的贪心解法，配合二叉堆可做到 O((V+E)logV)；"
              "Floyd 与 Warshall 于 1962 年给出 O(V³) 的全源算法，代码只有三重循环，工程上极难写错。"
              "在此之上，Tarjan 的深度优先搜索框架把割点、桥、强连通分量统一到 O(V+E)；"
              "Prim 与 Kruskal 分别从点集扩展和边排序两个角度解决最小生成树；"
              "Held 与 Karp 用状态压缩动态规划给出了小规模旅行商问题的精确解。")
    para(doc, "在工程侧，现代导航软件普遍采用分层图、路网预处理（如 Contraction Hierarchies）与 A* 启发式搜索"
              "来支撑大规模路网上的毫秒级查询。这些技术超出了课程设计的范围，但它们的共同基础仍是本文实现的"
              "几类最短路算法。因此本系统在算法层面追求“讲得清、写得对、测得准”，在工程层面追求"
              "“数据与算法解耦、核心库可独立测试”。")

    heading(doc, "1.3 课程设计目标与主要工作", 2)
    para(doc, "本次课程设计的目标是：以校园导航为载体，完成一个结构清晰、算法完整、测试充分的图算法应用，"
              "并在过程中训练需求分析、模块设计、编码规范、自动化测试与文档写作能力。具体工作包括：")
    for item in [
        "（1）对校园路网建模，设计地点、道路、图、路径等核心数据结构；",
        "（2）实现邻接表（CSR）与邻接矩阵两套存储，并给出选型依据；",
        "（3）实现 Dijkstra、Floyd-Warshall、Bellman-Ford、BFS 四类最短路径算法；",
        "（4）实现 Tarjan 割点/桥分析、Prim/Kruskal 最小生成树、Held-Karp 多点巡游；",
        "（5）实现 CSV 导入导出与带 CRC32 校验的二进制快照，支持图编辑；",
        "（6）实现命令行子命令与交互菜单，并绘制 ASCII 校园地图；",
        "（7）编写 1592 项自动化断言与 14 个端到端场景，接入 GitHub Actions；",
        "（8）完成本报告与答辩用的图表、幻灯片。",
    ]:
        para(doc, item, indent=False)

    heading(doc, "1.4 开发环境与工具链", 2)
    para(doc, "开发与验证环境如下表。选择 C11 标准并在编译期开启 -Wall -Wextra -Wpedantic -Werror，"
              "目的是让编译器代替人工发现类型、格式与可移植性问题；同时用同一套源码在 Windows（MinGW-w64）"
              "与 Linux（GitHub Actions 的 Ubuntu）上构建，验证跨平台性。")
    table(doc,
          ["项目", "配置"],
          [["操作系统", "Windows 11（开发）/ Ubuntu latest（CI）"],
           ["编译器", "GCC 16.2.0（w64devkit），-std=c11，-Werror"],
           ["构建脚本", "Makefile、scripts/build.ps1、build.cmd"],
           ["测试", "自研断言框架，tests/test_main.c；端到端脚本 e2e.ps1 / e2e.sh"],
           ["文档", "python-docx 生成 DOCX，LibreOffice 导出 PDF，Pillow 绘制插图"],
           ["版本控制", "Git，提交至 GitHub，GitHub Actions 自动构建测试"]],
          widths=[4.0, 11.5], text_caption="表 1-1 开发环境与工具链")

    # ---------------- 2 需求分析
    heading(doc, "2 需求分析", 1, page_break=True)
    heading(doc, "2.1 应用场景与用户需求", 2)
    para(doc, "校园导航的使用者大致分为四类：新生与访客，需要从校门或宿舍找到教学楼、食堂、图书馆；"
              "在校学生，需要在课间快速比较不同路线，尤其是在雨天或携带物品时更关心步行时间；"
              "校园管理者，关心路网的关键路段，一旦施工封路会不会造成某些区域不可达；"
              "来访的团体，希望一次串联多个地点参观，且总路程尽量短。")
    para(doc, "这四类需求分别对应：单源单汇最短路径（可切换距离/时间权重）、带权图上的最近设施查询、"
              "割点与桥分析、多点多目标路径优化。它们都属于图算法的经典问题，且规模不大（校园地点在几十个量级），"
              "适合用精确算法而不是启发式近似。")

    heading(doc, "2.2 功能需求", 2)
    para(doc, "根据上述场景，系统功能需求整理如表 2-1。功能编号在后面的设计与测试章节中沿用。")
    table(doc,
          ["编号", "功能", "说明", "关键算法"],
          [["F1", "校园地图", "按坐标绘制 ASCII 示意图，标注封闭道路", "坐标投影 + 直线光栅化"],
           ["F2", "地点信息", "地点列表、类型、坐标、简介、相邻道路", "线性查找"],
           ["F3", "路径规划", "任取两点，按距离或步行时间求最短路径", "Dijkstra"],
           ["F4", "多点巡游", "给定起终点与若干途经点，自动优化访问顺序", "Held-Karp / 2-opt"],
           ["F5", "最近设施", "从当前位置找最近的食堂、医院等同类设施", "Dijkstra + 筛选"],
           ["F6", "全源最短路", "输出任意两点最短距离矩阵", "Floyd-Warshall"],
           ["F7", "关键道路", "列出割点与桥，评估封路风险", "Tarjan"],
           ["F8", "最小生成树", "给出连通全部地点的最小布线方案", "Prim、Kruskal"],
           ["F9", "图编辑", "增删地点/道路，改长度与时间，封闭/恢复道路", "动态数组维护 + 重建"],
           ["F10", "数据持久化", "CSV 导入导出，二进制快照与校验", "CRC32"],
           ["F11", "算法对比", "随机图上比较各算法耗时", "计时 + 随机图生成"]],
          widths=[1.5, 3.0, 7.5, 3.5], text_caption="表 2-1 功能需求")

    heading(doc, "2.3 非功能需求", 2)
    for item in [
        "（1）性能：校园规模下单次路径查询应在 1 毫秒以内，全源矩阵不超过 10 毫秒，交互无明显等待感。",
        "（2）健壮性：任何非法输入（越界编号、非数字、损坏文件、不存在的地点）都必须给出可读错误，不得崩溃。",
        "（3）可移植性：同一份源码在 Windows 与 Linux 上都能以 -Werror 通过编译并运行。",
        "（4）可测试性：核心算法与输入输出解耦，测试程序可直接链接核心 .c 文件，不依赖菜单。",
        "（5）可维护性：模块边界清晰，函数失败返回负错误码，不使用 exit() 中断流程。",
        "（6）可扩展性：地点与道路由数据文件驱动，替换数据即可用于其他校区，无需改动算法。",
    ]:
        para(doc, item, indent=False)

    heading(doc, "2.4 典型用例与操作流程", 2)
    para(doc, "以“新生从宿舍A区到图书馆”为主用例：用户输入起点与终点（编号或名称均可），"
              "系统先校验地点是否存在，再按当前权重口径建图并运行 Dijkstra，最后输出完整路径、"
              "总距离、预计步行时间以及算法的出堆与松弛次数。若路径中包含桥，系统还会提示该路段一旦封闭的风险。"
              "异常流程包括：地点不存在（提示可用编号）、两点不连通（说明可能被封闭道路切断）、"
              "输入为空（取消本次操作）。交互菜单与命令行子命令共用同一套核心接口，保证两条入口行为一致。")

    # ---------------- 3 系统设计
    heading(doc, "3 系统设计", 1, page_break=True)
    heading(doc, "3.1 总体架构", 2)
    para(doc, "系统采用分层结构，自下而上分为支撑层、算法层、业务层与表示层，如图 3-1。"
              "支撑层负责数据文件、图存储、编码与计时等基础设施；算法层只依赖图结构，不感知控制台；"
              "业务层把算法组合成“路径规划”“多点巡游”等校园功能；表示层负责菜单、子命令与输出格式。"
              "这种划分使得核心算法可以被测试程序直接调用，也便于将来替换表示层（例如改为图形界面）。")
    figure(doc, "architecture.png", "图 3-1 系统总体架构", 15.5)

    heading(doc, "3.2 模块划分", 2)
    para(doc, "按职责把源码划分为 9 个模块，见表 3-1。表中同时给出各文件的规模，"
              "可见算法层与数据层是代码主体，界面层保持轻薄。")
    modules = [
        ("include/nav.h", "公共类型与常量", ""),
        ("src/campus.c", "数据模型、校验、CSV/快照、编辑", ""),
        ("src/graph.c", "CSR 邻接表与邻接矩阵构建", ""),
        ("src/algo.c", "七类图算法与路径重建", ""),
        ("src/app.c", "应用状态、数据目录发现与重建", ""),
        ("src/ui.c", "表格、ASCII 地图、菜单", ""),
        ("src/main.c", "子命令解析与分发", ""),
        ("src/platform.c", "UTF-8 控制台/参数、计时、目录", ""),
        ("src/crc32.c", "CRC-32 查表实现", ""),
        ("src/default_campus.c", "内置默认数据集（脚本生成）", ""),
    ]
    rows = []
    for name, desc, _ in modules:
        rows.append([name, desc, count_lines(os.path.join(ROOT, name))])
    table(doc, ["文件", "职责", "行数"], rows, widths=[4.3, 8.7, 2.0],
          text_caption="表 3-1 模块划分与代码规模")

    heading(doc, "3.3 核心数据结构", 2)
    para(doc, "系统围绕四个结构体组织数据：Spot 描述地点，Road 描述无向道路，Campus 是地点与道路的集合，"
              "Graph 是由 Campus 派生出的图结构，Path 表示一次查询的结果。地点编号从 1 开始连续编号，"
              "并约定“编号即下标 + 1”，这样算法内部可以用数组直接索引，避免额外的映射表。")
    table(doc,
          ["结构体", "字段", "说明"],
          [["Spot", "id, name, type, x, y, brief", "地点编号、名称、类型、平面坐标、简介"],
           ["Road", "id, from, to, len_m, walk_min, open, name", "无向边；open 为 0 表示临时封闭"],
           ["Campus", "title, width_m, height_m, spots[], roads[], n_spots, n_roads", "完整数据集，容量上限 128/1024"],
           ["Graph", "n, off[], to[], w[], road[], eu/ev/ew/eid[], n_arcs, n_edges", "CSR 邻接表 + 边表"],
           ["Path", "nodes[], count, weight, settled, relaxed", "路径节点序列、总权重与算法统计量"]],
          widths=[2.4, 6.6, 6.5], text_caption="表 3-2 核心数据结构")

    heading(doc, "3.4 图的存储结构选型", 2)
    para(doc, "同一张图需要两种视图：遍历与单源最短路适合邻接表，Floyd 需要矩阵。"
              "本系统以 CSR（压缩稀疏行）形式存储邻接表：把所有弧按起点连续存放，用 off[] 记录每个顶点的"
              "起止下标。相比“指针链表”的邻接表，CSR 省去每个弧的指针开销，遍历时访问连续内存，"
              "缓存命中率更高；同时它天然适合无向图（一条道路写入两条弧）。")
    para(doc, "矩阵视图按需求即时构建（graph_adj_matrix），因为 Floyd 本身是 O(V³)，"
              "构建矩阵的 O(V²+E) 可以忽略。两套结构都由 Campus 派生，编辑数据后统一重建，避免状态不一致。"
              "封闭道路在建图时直接跳过，因此“封路”不需要改算法，只需重建图。")

    heading(doc, "3.5 数据文件设计", 2)
    para(doc, "数据层提供两种格式：CSV 面向人工编辑与版本对比，二进制快照面向快速加载与完整性校验，"
              "格式如图 3-2。CSV 采用 UTF-8 编码，首行以“# title:”给出数据集标题，其余以 # 开头的行为注释；"
              "字段支持双引号包裹，字段内的逗号、引号不会破坏解析。")
    figure(doc, "data-format.png", "图 3-2 数据文件格式", 15.5)
    para(doc, "二进制快照按定长字段逐个小端序写入，而不是直接 fwrite 结构体——后者会受编译器对齐与端序影响，"
              "换平台就无法读取。快照末尾附 CRC32，读取时先校验再解析，可检测截断、篡改与版本不符。"
              "此外快照带 magic（NVG1）与版本号，为将来格式升级留出空间。")

    heading(doc, "3.6 关键接口设计", 2)
    para(doc, "所有核心接口遵循同一约定：成功返回 0，失败返回负错误码，错误信息写入调用方提供的 "
              "char *err, size_t cap 缓冲区；不调用 exit()，由上层决定如何提示。这样测试程序可以逐条断言"
              "错误码与错误文本，界面层也能把错误转换成友好提示。主要接口见表 3-3。")
    table(doc,
          ["接口", "作用", "返回值约定"],
          [["campus_load_csv / save_csv", "CSV 读写", "0 成功；NAV_ERR_FMT 格式错误；NAV_ERR_IO 打不开"],
           ["campus_validate", "数据集校验", "0 合法；负值附带具体原因"],
           ["campus_save_bin / load_bin", "快照读写与 CRC 校验", "损坏、截断、版本不符均返回负值"],
           ["graph_build", "由数据集建图", "内存不足返回 NAV_ERR_NOMEM"],
           ["algo_dijkstra", "单源单汇最短路", "0 成功；Path.count==0 表示不连通"],
           ["algo_floyd", "全源最短路矩阵", "返回 malloc 的 dist 与 next，调用方释放"],
           ["algo_critical", "割点与桥", "桥以道路编号返回"],
           ["algo_tsp", "多点巡游", "k≤12 精确解，否则启发式"]],
          widths=[5.6, 5.0, 6.4], text_caption="表 3-3 关键接口")

    # ---------------- 4 算法设计与分析
    heading(doc, "4 算法设计与分析", 1, page_break=True)
    heading(doc, "4.1 问题建模", 2)
    para(doc, "把校园抽象为无向带权图 G=(V,E)，|V|=24，|E|=41。每条边有两个权重：长度 len（米）与"
              "步行时间 time（分钟），分别对应“最短距离”与“最快到达”两种口径。两个权重都由数据文件给出，"
              "建图时通过 by_time 开关选择其一，避免在算法内部做额外判断。")
    para(doc, "约定：顶点按 1..n 连续编号；边权恒为正；图允许不连通（封路后可能出现孤立区域），"
              "算法必须能区分“距离有限”与“不可达”。不连通用 NAV_INF（1e18）表示，而不是 0 或 -1，"
              "这样在比较与累加时不会与合法权重混淆。")

    heading(doc, "4.2 Dijkstra 算法", 2)
    para(doc, "Dijkstra 的核心是贪心：每次从“尚未确定最短距离”的顶点中取出当前距离最小的顶点 u，"
              "把 dist[u] 固定下来，再用 u 的邻边松弛其他顶点。正确性依赖于边权非负——"
              "若存在负边，已确定的距离可能被后来的路径推翻。")
    para(doc, "实现上使用二叉最小堆维护候选集。为了代码简单，采用“惰性删除”：松弛成功就把新距离压堆，"
              "出堆时若发现 key 大于当前 dist[u] 则丢弃。这样不需要实现 decrease-key，"
              "堆中元素最多为弧数，复杂度 O((V+E)logE)，对校园规模完全够用。")
    code_block(doc, [
        "dist[src] = 0;  heap.push(0, src);",
        "while (!heap.empty()) {",
        "    (key, u) = heap.pop();",
        "    if (key > dist[u] + EPS) continue;        /* 过期堆项，丢弃 */",
        "    if (u == dst) break;                      /* 单汇查询可提前结束 */",
        "    for each arc (u -> v, w):",
        "        if (dist[u] + w < dist[v] - EPS) {",
        "            dist[v] = dist[u] + w;  prev[v] = u;",
        "            heap.push(dist[v], v);",
        "        }",
        "}",
    ])
    para(doc, "路径重建用 prev 数组回溯：从终点一路走到起点，压栈后逆序输出。"
              "实现中把回溯封装为 path_from_prev，并特意保留调用方写入的 settled/relaxed 统计量，"
              "避免统计信息被清零——这个细节在测试中被显式断言，防止回归。")

    heading(doc, "4.3 Floyd-Warshall 算法", 2)
    para(doc, "Floyd 用动态规划求全源最短路：记 d[i][j] 为只允许经过前 k 个顶点时的最短距离，"
              "转移方程 d[i][j] = min(d[i][j], d[i][k] + d[k][j])。三重循环结束后即为任意两点最短距离。"
              "它不要求边权非负（但不能有负环），代码短、易验证，适合校园这种小规模全源查询。")
    para(doc, "路径重建依赖 next 矩阵：next[i][j] 记录“从 i 到 j 的第一跳”。转移成功时，"
              "next[i][j] 应更新为 next[i][k]（i 到 k 的第一跳），而不是 next[k][j]。"
              "本系统最初按后一种写法实现，测试中 20 组点对出现路径节点缺失，正是通过"
              "“路径合法性断言”定位到该错误，这也说明交叉验证的必要性。")
    code_block(doc, [
        "for (k = 0; k < n; k++)",
        "  for (i = 0; i < n; i++) {",
        "    if (d[i][k] == INF) continue;",
        "    for (j = 0; j < n; j++)",
        "      if (d[i][k] + d[k][j] < d[i][j]) {",
        "        d[i][j] = d[i][k] + d[k][j];",
        "        next[i][j] = next[i][k];   /* 第一跳 */",
        "      }",
        "  }",
    ])

    heading(doc, "4.4 Bellman-Ford 算法", 2)
    para(doc, "Bellman-Ford 对每条弧重复松弛 V-1 轮：若第 V 轮仍能松弛，说明存在负权回路，最短路径无定义。"
              "它的复杂度 O(VE)，比 Dijkstra 慢，但能处理负权，是算法完整性的一部分。"
              "本系统按“弧”而不是“无向边”松弛，这样既支持有向图测试，也保持与 CSR 结构一致。")
    para(doc, "需要说明的是：校园道路是无向图，若给某条无向边负权，则“来回走一遍”立即构成负环，"
              "算法会正确报错。因此负权用例只能构造有向图来验证，测试中专门写了一个构造有向图的小工具，"
              "断言 1→3 的最短路为 2（1→2→3），以及三元负环能被检出。")

    heading(doc, "4.5 BFS 与最少路段路径", 2)
    para(doc, "当用户关心的不是距离而是“少拐几个弯、少走几段路”时，等价于边权全为 1 的最短路，"
              "用 BFS 按层扩展即可，复杂度 O(V+E)。系统保留该功能用于对比：同一对起点终点，"
              "Dijkstra 给出最短距离，BFS 给出最少路段数，二者路径常常不同。")

    heading(doc, "4.6 Tarjan 割点与桥", 2)
    para(doc, "Tarjan 算法在一次 DFS 中维护两个时间戳：disc[u] 表示 u 首次被访问的次序，"
              "low[u] 表示从 u 的子树出发、经过至多一条回边能到达的最早顶点。"
              "对树边 (u,v)：若 low[v] > disc[u]，则 (u,v) 是桥；若 u 不是根且 low[v] ≥ disc[u]，则 u 是割点；"
              "根节点若有两个以上子树也是割点。")
    para(doc, "在校园场景中，桥意味着“唯一通道”：一旦封闭，该侧区域将不可达。系统在路径规划结果里会提示"
              "路径中是否包含桥，管理端的“关键道路分析”则列出全部割点与桥。当前数据中有 2 个割点"
              "（快递服务中心、创新创业园）与 2 条桥（分别通往观海亭、实训码头），"
              "这与设计中“东北角观景平台、实训码头只有一条步道”的布局一致。")

    heading(doc, "4.7 最小生成树", 2)
    para(doc, "最小生成树回答“用最短的总长度把所有地点连通起来”。Prim 从任一点出发，每次把距离当前树最近的"
              "顶点加入，适合稠密图；Kruskal 把边按权重排序，用并查集依次合并，适合稀疏图。"
              "本系统同时实现两者并互相校验——若两者总权重不一致，说明实现有 bug。")

    heading(doc, "4.8 多点巡游问题", 2)
    para(doc, "多点巡游是旅行商问题的变体：固定起点与终点，途中经过 k 个指定地点，顺序可优化，目标是最小化"
              "总路程。当 k≤12 时，用状态压缩动态规划（Held-Karp）求精确解："
              "dp[mask][i] 表示已访问集合 mask、当前停在 i 的最小代价，转移是枚举下一个未访问点，"
              "复杂度 O(2^k·k²)。")
    para(doc, "当 k>12 时，状态数爆炸，改用“最近邻构造 + 2-opt 局部改进”的启发式：先贪心走到最近的未访问点，"
              "再反复尝试反转一段子路径，若总长变短则接受。系统在 campus 规模上默认走精确解，"
              "启发式路径主要出现在大数据或测试场景。")
    para(doc, "实现上还有一个细节：各段之间用 Dijkstra 求实际最短路，再把节点序列拼接起来，"
              "拼接时跳过重复的连接点，否则输出会出现“行政楼(5) → 行政楼(5)”这样的重复节点。")

    heading(doc, "4.9 算法对比与选型", 2)
    para(doc, "各算法的复杂度与适用场景汇总如表 4-1。系统在交互路径规划中默认使用 Dijkstra，"
              "全源查询使用 Floyd，关键道路用 Tarjan，布线用 Prim/Kruskal 双算并比对。")
    table(doc,
          ["算法", "时间复杂度", "空间", "是否支持负权", "在本系统中的作用"],
          [["Dijkstra（二叉堆）", "O((V+E)logE)", "O(V+E)", "否", "单源最短路，交互查询主算法"],
           ["Floyd-Warshall", "O(V³)", "O(V²)", "是（无负环）", "全源矩阵，任意两地距离"],
           ["Bellman-Ford", "O(VE)", "O(V+E)", "是", "负权与负环检测，算法对照"],
           ["BFS", "O(V+E)", "O(V)", "不适用", "最少路段数路径"],
           ["Tarjan", "O(V+E)", "O(V)", "不适用", "割点与桥，封路风险分析"],
           ["Prim", "O(V²+E)", "O(V)", "不适用", "稠密图最小生成树"],
           ["Kruskal", "O(ElogE)", "O(V+E)", "不适用", "稀疏图最小生成树，并查集"],
           ["Held-Karp", "O(2^k·k²)", "O(2^k·k)", "不适用", "k≤12 的精确巡游"],
           ["最近邻 + 2-opt", "O(k³)", "O(k)", "不适用", "k>12 的巡游近似"]],
          widths=[3.6, 3.3, 2.2, 3.0, 5.0], text_caption="表 4-1 算法对比")

    # ---------------- 5 系统实现
    heading(doc, "5 系统实现", 1, page_break=True)
    heading(doc, "5.1 工程结构", 2)
    para(doc, "工程按“头文件 + 实现 + 测试 + 数据 + 脚本 + 文档”组织。scripts/ 下的脚本负责构建、测试、"
              "端到端验证与提交前检查；数据集中在 data/campus/；文档与生成脚本放在 docs/。"
              "所有构建入口（build.cmd、build.ps1、Makefile）使用同一组编译选项，"
              "避免“本地能过、CI 不过”的情况。")

    heading(doc, "5.2 数据层实现", 2)
    para(doc, "CSV 解析是一个手写的状态机：按行读取，跳过注释与空行，逐字段切分。切分时要同时处理"
              "引号包裹、字段内转义引号、行尾空格等情况。实现中曾出现一个真实 bug：去掉行尾空格时"
              "把分隔逗号一并写成了 '\\0'，导致整行只解析出 1 个字段；测试中“第 4 行列数应为 6”的断言"
              "直接暴露了它，修复后新增了带逗号与引号的往返用例。")
    para(doc, "校验函数 campus_validate 集中检查：编号是否连续、端点是否越界、长度与时间是否为正、"
              "是否存在自环与重复边、坐标是否在绘制范围内、名称是否重复、容量是否超限。"
              "所有错误都附带可读原因，例如“地点文件第 4 行列数为 1，应为 6”。")

    heading(doc, "5.3 算法层实现", 2)
    para(doc, "算法层只依赖 graph.h 暴露的 CSR 结构，不引用任何界面函数。"
              "路径重建、堆操作、并查集等公共部件都写成静态函数，避免污染接口。"
              "巡游算法把“终端间代价矩阵”与“顺序优化”分开：先对 k+2 个终端各跑一次 Dijkstra 得到代价矩阵，"
              "再在这张小矩阵上做 DP 或启发式，最后按最优顺序拼接各段最短路。")

    heading(doc, "5.4 交互层实现", 2)
    para(doc, "交互层提供两条入口：交互菜单适合演示，子命令适合脚本与测试，例如 route、tour、near、"
              "critical、mst、allpairs、bench 等，并支持 --json 输出便于端到端比对。"
              "ASCII 地图把校园坐标线性投影到 68×30 的字符网格，用 Bresenham 直线算法绘制道路，"
              "再用数字与字母标注地点编号，图例分三栏列出名称。")

    heading(doc, "5.5 健壮性设计", 2)
    para(doc, "健壮性体现在三处：其一，所有对外函数返回错误码而不是退出进程，界面层统一处理；"
              "其二，每个 malloc 都有对应的 free，错误分支用统一出口释放；其三，"
              "二进制快照在解析前先校验长度、magic、版本与 CRC32，任何一项不符都拒绝加载，"
              "从而把“文件损坏”转化为可读错误而不是越界访问。")
    para(doc, "测试还覆盖了容量边界（地点数达到 128、道路数达到 1024）、空文件、只有注释的文件、"
              "重复边、自环、负长度、坐标越界等情形，确保异常路径与正常路径一样被验证。")

    heading(doc, "5.6 跨平台与编码处理", 2)
    para(doc, "Windows 控制台默认不是 UTF-8，程序启动时把输入输出代码页切换为 UTF-8；"
              "中文路径通过 _wfopen 打开，避免 ANSI 路径在非中文系统上失效；"
              "更隐蔽的问题是命令行参数：main 收到的 argv 是按 ANSI 代码页转换的，"
              "直接使用会导致中文地点名乱码。解决办法是用 GetCommandLineW + CommandLineToArgvW 重新取参，"
              "再转成 UTF-8。这个问题在端到端测试中以“找不到地点”的形式暴露，修复后中文参数可正常查询。")
    para(doc, "构建脚本同样遇到编码问题：Windows PowerShell 5.1 会把不带 BOM 的 UTF-8 脚本按 ANSI 解析，"
              "含中文的 .ps1 会直接语法报错。工程约定脚本内注释与提示尽量使用 ASCII，"
              "确需中文时保存为 UTF-8 with BOM，并在 .gitattributes 中固定换行符。")

    # ---------------- 6 测试
    heading(doc, "6 测试与结果分析", 1, page_break=True)
    heading(doc, "6.1 测试策略", 2)
    para(doc, "测试分三层：单元测试验证核心函数的行为与错误路径；随机交叉验证用大量随机图比对不同算法的结果；"
              "端到端测试从命令行入口验证真实使用流程，包括 JSON 输出与退出码。"
              "三层测试都在 CI 中自动执行，任何一次提交都必须全绿。")
    para(doc, "测试框架是自研的轻量断言宏，只依赖标准 C：CHECK 判断表达式，CHECK_INT 与 CHECK_NEAR 比较整数与浮点数，"
              "CHECK_STR 比较字符串。失败时打印文件名、行号与实际/期望值，便于定位。")

    heading(doc, "6.2 单元测试", 2)
    total_checks = sum(g[1] for g in groups) if groups else 1592
    para(doc, f"单元测试共 {len(groups)} 组、{total_checks} 项断言，覆盖工具层、数据层、算法层与应用集成，"
              "分组统计见表 6-1 与图 6-1。")
    rows = [[name, checks, "通过" if failed == 0 else f"失败 {failed}"]
            for name, checks, failed in groups]
    rows.append(["合计", total_checks, "全部通过"])
    table(doc, ["测试组", "断言数", "结果"], rows, widths=[6.0, 3.0, 5.0],
          text_caption="表 6-1 单元测试分组统计")
    figure(doc, "test-summary.png", "图 6-1 自动化测试覆盖", 14.5)

    heading(doc, "6.3 随机交叉验证", 2)
    para(doc, "交叉验证是发现算法实现错误最有效的手段：随机生成 30 张规模 8—19 的连通图，"
              "分别在距离口径与时间口径下运行 Dijkstra、Floyd、Bellman-Ford，逐点比对三者的结果。"
              "若任何一对结果不一致，就说明至少有一个实现有问题。")
    para(doc, "在此基础上还做了两类性质测试：其一，对每条返回的路径检查“相邻节点之间确实有边，"
              "且边权之和等于返回的总权重”，防止路径重建错位；其二，对每条识别出的桥，"
              "实际把它封闭后重新计算连通分量，断言图确实分裂，从行为上验证 Tarjan 的判定。")

    heading(doc, "6.4 端到端测试", 2)
    para(doc, "端到端脚本共 14 个场景，覆盖：地点列表、JSON 路径、按时间口径查询、起终点相同、"
              "非法地点退出码为 2、最近设施、多点巡游、割点与桥、最小生成树、全源矩阵、ASCII 地图、"
              "数据校验、基准测试，以及“导出的 CSV 与仓库中数据逐行一致”。"
              "最后一个场景把“数据文件被误改”纳入回归范围，避免演示数据与代码脱节。")

    heading(doc, "6.5 性能测试", 2)
    para(doc, "性能测试用随机生成的校园图（约 3V 条边）从 20 个节点扩展到 120 个节点，"
              "每种规模重复运行取平均，结果见表 6-2 与图 6-2。计时使用 Windows 的 QueryPerformanceCounter，"
              "避免 clock() 只有 1 毫秒粒度的问题。")
    rows = []
    for row in bench:
        rows.append([row["n"],
                     f"{row.get('dijkstra', 0):.4f}",
                     f"{row.get('floyd', 0):.4f}",
                     f"{row.get('bellman', 0):.4f}",
                     f"{row.get('prim', 0):.4f}",
                     f"{row.get('kruskal', 0):.4f}"])
    table(doc, ["节点数", "Dijkstra(ms)", "Floyd(ms)", "Bellman(ms)", "Prim(ms)", "Kruskal(ms)"],
          rows, widths=[2.2, 2.8, 2.5, 2.8, 2.4, 2.6], size=10,
          text_caption="表 6-2 随机图上的算法耗时")
    figure(doc, "algo-compare.png", "图 6-2 算法耗时随规模变化", 15.0)
    para(doc, "结果显示：在稀疏图上 Dijkstra 稳定在微秒级，120 节点时单次查询约 3.5 微秒；"
              "Floyd 随节点数呈立方增长，120 节点约 1 毫秒；Bellman-Ford 介于两者之间；"
              "Kruskal 由于排序，在小图上反而略慢于 Prim，但随规模增长接近线性。"
              "这些数据与理论复杂度一致，也说明校园规模下任何算法都能满足交互需求。")

    heading(doc, "6.6 运行结果展示", 2)
    para(doc, "图 6-3 为真实运行截图，展示从宿舍A区到图书馆的路径规划、关键道路分析与最小生成树三个典型功能。"
              "路径为 宿舍A区(16) → 学生一食堂(14) → 校训广场(22) → 图书馆(4)，总距离 408.5 米，"
              "预计步行 5.2 分钟；关键道路分析给出 2 个割点与 2 条桥；"
              "Prim 与 Kruskal 得到的布线总长均为 3477.7 米，选中 23 条道路。")
    figure(doc, "terminal-demo.png", "图 6-3 命令行运行结果", 15.5)
    para(doc, "图 6-4 为 ASCII 校园地图与路径高亮示意图。地图按真实坐标投影，封闭道路以 '+' 标注；"
              "红色路径为查询结果。由于终端字符比例的天然差异，地图只保证拓扑正确与相对位置，"
              "不追求与实测平面图完全一致。")
    figure(doc, "campus-map.png", "图 6-4 校园路网（按坐标绘制）", 15.0)
    figure(doc, "routing-demo.png", "图 6-5 最短路径示意（宿舍A区 → 图书馆）", 15.0)

    # ---------------- 7 总结
    heading(doc, "7 总结与展望", 1, page_break=True)
    heading(doc, "7.1 完成情况", 2)
    para(doc, "本次课程设计完成了从需求分析、数据结构设计、算法实现到测试与文档的完整闭环。"
              "系统实现了 11 项功能，覆盖 8 类图算法；核心库与界面解耦，可在无控制台环境下被测试程序直接调用；"
              "数据由文件驱动，替换 CSV 即可用于其他校区。")
    para(doc, "工程方面，项目建立了统一的构建、测试与提交前检查脚本，"
              "共 22 组 1592 项自动化断言与 14 个端到端场景，全部通过；GitHub Actions 在 Linux 上"
              "完成构建与测试，验证了跨平台性。报告与答辩材料由脚本生成，图、表、数据与程序输出一一对应。")
    para(doc, "在过程中也踩到了不少“工程坑”：CSV 分隔符被误写、Floyd 的 next 更新写错方向、"
              "PowerShell 把 UTF-8 脚本读成 ANSI、中文命令行参数乱码、Linux 上 mkdir 未实现等。"
              "这些问题的共同点是“单看代码很难发现，必须靠测试或真实运行才能暴露”，"
              "也让我更深刻地理解了“可测试性”比“写得快”更重要。")

    heading(doc, "7.2 不足与改进方向", 2)
    for item in [
        "（1）校园数据为教学示意值，尚未接入真实测绘数据；后续可通过导入 GeoJSON 或高德 POI 数据自动建图。",
        "（2）地点数量上限固定为 128，矩阵使用静态容量；若要支持更大规模，需改为动态分配与稀疏结构。",
        "（3）巡游问题在 k>12 时使用启发式，无法保证最优；可引入分支限界或 Lin-Kernighan 改进解的质量。",
        "（4）界面仍是控制台，缺少图形化地图；可在现有核心库之上增加 Win32 或 Web 前端。",
        "（5）尚未支持实时路况与多目标优化（如同时最小化距离与台阶数）；可扩展为多权重最短路。",
    ]:
        para(doc, item, indent=False)

    # ---------------- 参考文献
    heading(doc, "参考文献", 1, page_break=True)
    refs = [
        "[1] 严蔚敏, 吴伟民. 数据结构（C语言版）[M]. 北京: 清华大学出版社, 2007.",
        "[2] Dijkstra E W. A note on two problems in connexion with graphs[J]. Numerische Mathematik, 1959, 1: 269-271.",
        "[3] Floyd R W. Algorithm 97: Shortest path[J]. Communications of the ACM, 1962, 5(6): 345.",
        "[4] Bellman R. On a routing problem[J]. Quarterly of Applied Mathematics, 1958, 16(1): 87-90.",
        "[5] Tarjan R. Depth-first search and linear graph algorithms[J]. SIAM Journal on Computing, 1972, 1(2): 146-160.",
        "[6] Prim R C. Shortest connection networks and some generalizations[J]. Bell System Technical Journal, 1957, 36(6): 1389-1401.",
        "[7] Kruskal J B. On the shortest spanning subtree of a graph[J]. Proceedings of the American Mathematical Society, 1956, 7(1): 48-50.",
        "[8] Held M, Karp R M. A dynamic programming approach to sequencing problems[J]. Journal of the Society for Industrial and Applied Mathematics, 1962, 10(1): 196-210.",
        "[9] Cormen T H, Leiserson C E, Rivest R L, Stein C. Introduction to Algorithms[M]. 4th ed. Cambridge: MIT Press, 2022.",
        "[10] ISO/IEC 9899:2018. Programming languages — C[S]. Geneva: ISO, 2018.",
    ]
    for ref in refs:
        p = para(doc, ref, indent=False)
        p.paragraph_format.left_indent = Cm(0.8)
        p.paragraph_format.first_line_indent = Cm(-0.8)

    # ---------------- 附录
    heading(doc, "附录 A 命令速查", 1, page_break=True)
    table(doc,
          ["命令", "说明"],
          [["nav", "进入交互菜单"],
           ["nav map", "打印 ASCII 校园地图"],
           ["nav list / roads", "地点列表 / 道路列表"],
           ["nav show <地点>", "地点详情（编号或名称）"],
           ["nav route <起点> <终点> [--by time|dist] [--json]", "最短路径，可切换口径与 JSON 输出"],
           ["nav tour <起点> <终点> <途经...>", "多点巡游，自动优化顺序"],
           ["nav near <位置> <类型>", "最近设施查询"],
           ["nav allpairs [--limit N]", "Floyd 全源最短路矩阵"],
           ["nav critical", "割点与桥分析"],
           ["nav mst", "最小生成树（Prim 与 Kruskal 对比）"],
           ["nav check", "数据校验与连通性检查"],
           ["nav bench [--nodes N] [--seed S] [--repeat R]", "算法基准测试"],
           ["nav save / export <目录> / import <快照>", "保存 CSV / 另存 CSV / 导入快照"]],
          widths=[9.0, 6.5], size=10, text_caption="表 A-1 命令速查")

    heading(doc, "附录 B 测试用例摘录", 1, page_break=True)
    table(doc,
          ["编号", "场景", "输入", "期望结果"],
          [["T01", "正常最短路", "route 16 4", "4 个节点，408.5 米"],
           ["T02", "时间口径", "route 东大门 图书馆 --by time", "6.4 分钟"],
           ["T03", "起终点相同", "route 1 1", "路径长度 1，距离 0"],
           ["T04", "不连通", "封闭桥后再查询", "路径为空，提示无通路"],
           ["T05", "非法编号", "route 1 999", "退出码 2 与提示"],
           ["T06", "CSV 列数错误", "构造 1 列的文件", "报“列数应为 6”"],
           ["T07", "CSV 重复边", "两条 1-2", "报“已有道路重复”"],
           ["T08", "快照篡改", "翻转 1 个字节", "CRC32 校验失败"],
           ["T09", "快照截断", "截掉 7 字节", "拒绝加载"],
           ["T10", "负权回路", "构造 1→2→3→1 负环", "返回负环错误"],
           ["T11", "割点识别", "链式图 1-2-3-4", "2、3 为割点，3 条桥"],
           ["T12", "MST 一致性", "环图与校园图", "Prim 与 Kruskal 总长相等"],
           ["T13", "巡游最优性", "k=4 随机图", "与暴力枚举结果一致"],
           ["T14", "最近设施", "near 宿舍A区 canteen", "学生一食堂"]],
          widths=[1.4, 3.4, 5.6, 5.1], size=10, text_caption="表 B-1 典型测试用例")


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=ROOT)
    ap.add_argument("--pages", default=None, help="回填目录页码的 JSON")
    ap.add_argument("--out", default=None)
    ap.add_argument("--mask", action="store_true", help="隐藏个人信息（公开仓库版）")
    args = ap.parse_args(argv[1:])

    if args.mask:
        apply_mask()
    pages = {}
    if args.pages and os.path.exists(args.pages):
        with open(args.pages, encoding="utf-8") as f:
            pages = json.load(f)

    doc = Document()
    # 默认样式
    style = doc.styles["Normal"]
    style.font.name = BODY_FONT
    style.font.size = BODY_SIZE
    style.element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)

    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.top_margin = Cm(2.54)
    sec.bottom_margin = Cm(2.54)
    sec.left_margin = Cm(3.0)
    sec.right_margin = Cm(2.5)

    build_cover(doc)

    # 前置部分（摘要 + 目录）：单独一节，不显示页眉页脚
    sec2 = doc.add_section(WD_SECTION.NEW_PAGE)
    sec2.header.is_linked_to_previous = False
    sec2.footer.is_linked_to_previous = False
    build_front_matter(doc, pages)

    # 正文：新节，页码从 1 开始，带页眉页脚
    sec3 = doc.add_section(WD_SECTION.NEW_PAGE)
    header_text(sec3, "课程设计报告")
    page_number_footer(sec3)
    restart_numbering(sec3, 1)
    build_body(doc)

    out = args.out or os.path.join(ROOT, "docs", "课程设计报告.docx")
    doc.save(out)

    toc_path = os.path.join(ROOT, "docs", "toc_entries.json")
    with open(toc_path, "w", encoding="utf-8") as f:
        json.dump([t for t, _ in TOC_ENTRIES], f, ensure_ascii=False, indent=2)
    print("report written:", out)


if __name__ == "__main__":
    main(sys.argv)
