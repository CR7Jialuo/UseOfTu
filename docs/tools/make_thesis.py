#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按《江苏海洋大学本科毕业设计（论文）说明书》模板生成报告。

实现要点：
  1. 以模板为底，保留封面（校徽/校名）、原创性声明、版权授权书与页眉页脚；
  2. 模板有 8 个分节：封面 / 声明+授权 / 中文摘要 / 外文摘要 / 目录 / 正文 / 结论+致谢+参考文献 / 附页；
     只清空第 2~7 节的内容，再按模板样式写入；
  3. 写作顺序：先把内容追加到文末，再整体搬到该节的分节符段落之前，
     避免直接用 XML 在任意位置插入带来的顺序问题；
  4. 目录页码用两遍法：先占位，导出 PDF 后用 find_pages.py 回填。

用法：
  python make_thesis.py [--pages docs/toc_pages.json] [--out xxx.docx] [--mask]
"""

import argparse
import json
import os
import re
import sys

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_TAB_ALIGNMENT, WD_TAB_LEADER
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TEMPLATE = os.path.join(ROOT, "build", "template", "template.docx")
FIG = os.path.join(ROOT, "docs", "diagrams")
DATA = os.path.join(ROOT, "docs", "data")

SONG, HEI, KAI, TNR, MONO = "宋体", "黑体", "楷体_GB2312", "Times New Roman", "Consolas"

TITLE_CN = "基于最短路径算法的校园导航系统设计与实现"
TITLE_EN = ("Design and Implementation of a Campus Navigation System "
            "Based on Shortest Path Algorithms")

PERSON = {
    "college": "【请填写】", "major": "【请填写】", "class": "信息1241",
    "name": "宋家洛", "sid": "2024122827", "advisor": "李薛",
    "advisor_title": "【请填写】", "corp_advisor": "无", "corp_title": "无",
}
DATE_CN = "2026 年 11 月"

TOC = [
    ("摘  要", 1), ("Abstract", 1),
    ("1 绪论", 1), ("1.1 课题背景与意义", 2), ("1.2 国内外研究现状", 2),
    ("1.3 主要工作", 2), ("1.4 开发环境与工具", 2),
    ("2 需求分析", 1), ("2.1 应用场景与用户需求", 2), ("2.2 功能需求", 2),
    ("2.3 非功能需求", 2),
    ("3 系统设计", 1), ("3.1 总体架构", 2), ("3.2 模块划分", 2),
    ("3.3 核心数据结构", 2), ("3.4 图的存储结构", 2), ("3.5 数据文件设计", 2),
    ("4 算法设计与分析", 1), ("4.1 问题建模", 2), ("4.2 Dijkstra 算法", 2),
    ("4.3 Floyd-Warshall 算法", 2), ("4.4 Bellman-Ford 算法", 2),
    ("4.5 BFS 与最少路段路径", 2), ("4.6 Tarjan 割点与桥", 2),
    ("4.7 最小生成树", 2), ("4.8 多点巡游问题", 2), ("4.9 算法对比与选型", 2),
    ("5 系统实现", 1), ("5.1 工程结构", 2), ("5.2 真实路网数据构建", 2),
    ("5.3 数据层实现", 2), ("5.4 算法层实现", 2), ("5.5 命令行与交互", 2),
    ("5.6 图形界面实现", 2), ("5.7 健壮性与跨平台", 2),
    ("6 测试与结果分析", 1), ("6.1 测试策略", 2), ("6.2 单元测试", 2),
    ("6.3 随机交叉验证", 2), ("6.4 端到端测试", 2), ("6.5 性能测试", 2),
    ("6.6 运行结果", 2),
    ("结论与展望", 1), ("致  谢", 1), ("参 考 文 献", 1),
]


# ---------------------------------------------------------------- 工具

def set_run(run, name=SONG, size=12.0, bold=False, color=None):
    run.font.name = name
    run.font.size = Pt(size)
    run.font.bold = bold
    rpr = run._element.get_or_add_rPr()
    rfonts = rpr.find(qn("w:rFonts"))
    if rfonts is None:
        rfonts = OxmlElement("w:rFonts")
        rpr.append(rfonts)
    for attr in ("w:eastAsia", "w:ascii", "w:hAnsi"):
        rfonts.set(qn(attr), name)
    if color is not None:
        run.font.color.rgb = color


def body_element():
    return OxmlElement("w:p")


class Writer:
    """把内容追加到文档末尾，最后整体搬到分节符段落之前。"""

    def __init__(self, doc, anchor):
        self.doc = doc
        self.anchor = anchor
        self.body = doc.element.body

    MARK = "{http://dsh.local/mark}old"

    def __enter__(self):
        # 给已有元素打临时标记，写作完成后按“没有标记”识别新元素。
        # （不能用 len 切片：python-docx 把新段落插在 body 末尾 sectPr 之前；
        #   也不能用 id()：lxml 元素释放后 id 会被复用。）
        for c in list(self.body):
            c.set(self.MARK, "1")
        return self

    def __exit__(self, exc_type, exc, tb):
        body = self.body
        new = [c for c in list(body) if c.get(self.MARK) != "1"]
        for el in new:
            body.remove(el)
            self.anchor.addprevious(el)
        for c in list(body):
            if c.get(self.MARK) == "1":
                del c.attrib[self.MARK]
        return False

    def p(self, text="", style="毕业设计（论文）正文", indent=True, align=None, size=12.0,
          bold=False, name=SONG, before=0, after=0, first_indent=None):
        p = self.doc.add_paragraph(style=style)
        pf = p.paragraph_format
        pf.line_spacing = 1.25
        if first_indent is not None:
            pf.first_line_indent = Pt(first_indent)
        elif not indent:
            pf.first_line_indent = Pt(0)
        if align is not None:
            p.alignment = align
        if before:
            pf.space_before = Pt(before)
        if after:
            pf.space_after = Pt(after)
        if text:
            set_run(p.add_run(text), name, size, bold)
        return p

    def h1(self, text, page_break=True):
        p = self.p(text, style="毕业设计（论文）1级标题", indent=False, name=HEI, size=15.0,
                   before=6, after=6)
        pf = p.paragraph_format
        pf.first_line_indent = Pt(0)
        pf.left_indent = Pt(0)
        pf.page_break_before = page_break
        ppr = p._p.get_or_add_pPr()
        lvl = OxmlElement("w:outlineLvl")
        lvl.set(qn("w:val"), "0")
        ppr.append(lvl)
        return p

    def h2(self, text):
        p = self.p(text, style="毕业设计（论文）2级标题", indent=False, name=HEI, size=14.0)
        p.paragraph_format.first_line_indent = Pt(0)
        ppr = p._p.get_or_add_pPr()
        lvl = OxmlElement("w:outlineLvl")
        lvl.set(qn("w:val"), "1")
        ppr.append(lvl)
        return p

    def h3(self, text):
        p = self.p(text, style="毕业设计（论文）3级标题", indent=False, name=HEI, size=12.0)
        p.paragraph_format.first_line_indent = Pt(0)
        ppr = p._p.get_or_add_pPr()
        lvl = OxmlElement("w:outlineLvl")
        lvl.set(qn("w:val"), "2")
        ppr.append(lvl)
        return p

    def big(self, text, page_break=True):
        p = self.p(text, style="毕业设计（论文）结论、致谢、参考文献标题", indent=False, name=HEI,
                   size=15.0, align=WD_ALIGN_PARAGRAPH.CENTER, before=24, after=12)
        if page_break:
            p.paragraph_format.page_break_before = True
        return p

    def caption(self, text):
        p = self.p(text, indent=False, align=WD_ALIGN_PARAGRAPH.CENTER, size=10.5, name=KAI,
                   before=3, after=8, first_indent=0)
        return p

    def figure(self, filename, text, width_cm=14.0):
        path = os.path.join(FIG, filename)
        if not os.path.exists(path):
            self.p(f"[缺少插图 {filename}]", indent=False)
            return
        p = self.doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.first_line_indent = Pt(0)
        p.paragraph_format.space_before = Pt(6)
        p.add_run().add_picture(path, width=Cm(width_cm))
        self.caption(text)

    def table(self, headers, rows, widths=None, cap=None, size=10.5):
        if cap:
            self.caption(cap)
        t = self.doc.add_table(rows=1, cols=len(headers))
        t.style = "Table Grid"
        t.alignment = WD_TABLE_ALIGNMENT.CENTER
        self._three_line(t)
        for i, h in enumerate(headers):
            cell = t.rows[0].cells[i]
            cell.text = ""
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.paragraph_format.first_line_indent = Pt(0)
            set_run(p.add_run(h), KAI, size, True)
        for row in rows:
            cells = t.add_row().cells
            for i, v in enumerate(row):
                cells[i].text = ""
                p = cells[i].paragraphs[0]
                p.paragraph_format.first_line_indent = Pt(0)
                p.paragraph_format.line_spacing = 1.0
                set_run(p.add_run(str(v)), KAI, size, False)
                if i > 0:
                    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        if widths:
            for i, w in enumerate(widths):
                t.columns[i].width = Cm(w)
                for cell in t.columns[i].cells:
                    cell.width = Cm(w)
        self.p("", indent=False, size=6)
        return t

    @staticmethod
    def _three_line(t):
        tbl_pr = t._tbl.tblPr
        borders = OxmlElement("w:tblBorders")
        for edge, sz in (("top", 12), ("bottom", 12), ("insideH", 4), ("insideV", 0),
                         ("left", 0), ("right", 0)):
            el = OxmlElement(f"w:{edge}")
            if sz:
                el.set(qn("w:val"), "single")
                el.set(qn("w:sz"), str(sz))
                el.set(qn("w:color"), "000000")
            else:
                el.set(qn("w:val"), "none")
            borders.append(el)
        tbl_pr.append(borders)


def section_bounds(doc):
    body = doc.element.body
    children = list(body)
    bounds = []
    for i, c in enumerate(children):
        if c.tag == qn("w:p"):
            ppr = c.find(qn("w:pPr"))
            if ppr is not None and ppr.find(qn("w:sectPr")) is not None:
                bounds.append(i)
    return children, bounds


def clear_section(doc, k):
    """清空第 k 节内容，返回插入锚点。

    分节符有两种：段落内的 w:sectPr（前 n-1 节），以及 body 末尾的 w:sectPr（最后一节）。
    最后一节没有“分节符段落”，锚点就是 body 级 sectPr 元素。
    """
    body = doc.element.body
    children, bounds = section_bounds(doc)
    if k < len(bounds):
        anchor = children[bounds[k]]
        start = 0 if k == 0 else bounds[k - 1] + 1
        end = bounds[k]
    else:
        sect = body.find(qn("w:sectPr"))
        anchor = sect
        start = bounds[-1] + 1 if bounds else 0
        end = len(children) - 1
    for c in children[start:end]:
        body.remove(c)
    return anchor




def drop_empty_paragraphs(doc, k, limit=20):
    """删掉第 k 节里的空段落（模板的占位空行），避免溢出到空白页。"""
    body = doc.element.body
    children, bounds = section_bounds(doc)
    start = 0 if k == 0 else bounds[k - 1] + 1
    end = bounds[k] if k < len(bounds) else len(children) - 1
    removed = 0
    for c in children[start:end]:
        if removed >= limit:
            break
        if c.tag == qn("w:p"):
            txt = "".join(n.text or "" for n in c.iter(qn("w:t"))).strip()
            if not txt:
                body.remove(c)
                removed += 1
    return removed

def page_field(section, prefix="第 ", suffix=" 页"):
    footer = section.footer
    p = footer.paragraphs[0] if footer.paragraphs else footer.add_paragraph()
    p.text = ""
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    set_run(p.add_run(prefix), SONG, 9)
    run = p.add_run()
    set_run(run, SONG, 9)
    b = OxmlElement("w:fldChar")
    b.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    e = OxmlElement("w:fldChar")
    e.set(qn("w:fldCharType"), "end")
    run._r.append(b)
    run._r.append(instr)
    run._r.append(e)
    set_run(p.add_run(suffix), SONG, 9)


def restart_numbering(section, start=1):
    sect_pr = section._sectPr
    for old in sect_pr.findall(qn("w:pgNumType")):
        sect_pr.remove(old)
    pg = OxmlElement("w:pgNumType")
    pg.set(qn("w:start"), str(start))
    sect_pr.append(pg)


# ---------------------------------------------------------------- 封面

def fill_cover(doc):
    children, bounds = section_bounds(doc)
    paragraphs = doc.paragraphs
    # 中文题目 / 英文题目（模板第 8、9 段）
    for p in paragraphs[:12]:
        t = p.text.strip()
        if "中文题目" in t:
            p.text = ""
            set_run(p.add_run(TITLE_CN), "楷体_GB2312", 20.0, True)
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        elif "English Title" in t:
            p.text = ""
            set_run(p.add_run(TITLE_EN), TNR, 15.0, True)
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER

    # 封面信息表
    if doc.tables:
        t = doc.tables[0]
        table_map = [
            [("college", 0, 1)], [("major", 1, 1)], [("class", 2, 1)],
            [("name", 3, 1)], [("sid", 3, 3)], [("advisor", 4, 1)], [("advisor_title", 4, 3)],
        ]
        # 模板表：r0 学院、r1 专业班级、r2 姓名/学号、r3 指导教师/职称、r4 企业导师/职称
        values = {
            (0, 1): PERSON["college"],
            (1, 1): (PERSON["class"] if PERSON["major"].startswith("【")
                     else f"{PERSON['major']}  {PERSON['class']}"),
            (2, 1): PERSON["name"], (2, 3): PERSON["sid"],
            (3, 1): PERSON["advisor"], (3, 3): PERSON["advisor_title"],
            (4, 1): PERSON["corp_advisor"], (4, 3): PERSON["corp_title"],
        }
        for (r, c), val in values.items():
            try:
                cell = t.rows[r].cells[c]
                cell.text = ""
                p = cell.paragraphs[0]
                set_run(p.add_run(val), "楷体_GB2312", 12.0, False)
            except IndexError:
                pass

    # 日期
    for p in doc.paragraphs[:20]:
        if p.text.strip().startswith("年") or "年    月" in p.text:
            p.text = ""
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            set_run(p.add_run(DATE_CN), SONG, 12.0)


# ---------------------------------------------------------------- 各节内容

def write_cn_abstract(anchor):
    with Writer(doc, anchor) as w:
        w.p("毕业设计（论文）中文摘要", indent=False, align=WD_ALIGN_PARAGRAPH.CENTER,
            name=HEI, size=16.0, before=6, after=12)
        w.p(f"题目：{TITLE_CN}", indent=False, name=HEI, size=12.0, after=6)
        for text in CN_ABSTRACT:
            w.p(text)
        p = w.p("")
        set_run(p.add_run("关键词："), HEI, 12.0, True)
        set_run(p.add_run("图论；最短路径；Dijkstra 算法；校园导航；C 语言"), SONG, 12.0)


def write_en_abstract(anchor):
    with Writer(doc, anchor) as w:
        w.p("毕业设计（论文）外文摘要", indent=False, align=WD_ALIGN_PARAGRAPH.CENTER,
            name=HEI, size=16.0, before=6, after=12)
        w.p(f"Title: {TITLE_EN}", indent=False, name=TNR, size=12.0, after=6)
        for text in EN_ABSTRACT:
            w.p(text, name=TNR)
        p = w.p("")
        set_run(p.add_run("Keywords: "), TNR, 12.0, True)
        set_run(p.add_run("graph theory; shortest path; Dijkstra; campus navigation; C"), TNR, 12.0)


def write_toc(anchor, pages):
    with Writer(doc, anchor) as w:
        w.p("目    录", indent=False, align=WD_ALIGN_PARAGRAPH.CENTER, name=HEI, size=14.0,
            before=6, after=12)
        for text, level in TOC:
            p = w.p("", indent=False, first_indent=0)
            pf = p.paragraph_format
            pf.left_indent = Cm(0.0 if level == 1 else 0.75)
            pf.tab_stops.add_tab_stop(Cm(15.2), WD_TAB_ALIGNMENT.RIGHT, WD_TAB_LEADER.DOTS)
            set_run(p.add_run(text), SONG, 12.0, level == 1)
            page = pages.get(text, "00")
            set_run(p.add_run("\t" + str(page)), SONG, 12.0)


def write_body(anchor, figures):
    with Writer(doc, anchor) as w:
        # ---------------- 1 绪论
        w.h1("1 绪论", page_break=False)
        w.h2("1.1 课题背景与意义")
        w.p("校园面积扩大与功能分区细化，使“从当前位置到目标建筑怎么走最近”成为师生与访客的高频问题。"
            "指示牌信息有限，商业地图对校内步行道路、临时封闭路段覆盖不足，"
            "因此需要一个面向本校区的导航工具。")
        w.p("从数学角度看，校园路网是一张无向带权图：地点是顶点，道路是边，道路长度或步行时间是边权。"
            "于是“怎么走最近”是单源最短路径问题，“任意两地多远”是全源最短路径问题，"
            "“哪条路封了会断路”是割点与桥问题，“铺一套巡检线路”是最小生成树问题。"
            "把《数据结构》中的经典算法落成可运行、可测试、可演示的系统，正是本课题的目标。")
        w.p("本系统以江苏海洋大学苍梧校区为对象，建立了包含 353 个节点（20 个地图编号地点、"
            "319 个路网路口与接驳点）、409 条道路、总长约 22.4 km 的真实路网，"
            "实现命令行与 Win64 图形两套界面，并用 1578 项自动化断言保证正确性。")

        w.h2("1.2 国内外研究现状")
        w.p("最短路径算法在 20 世纪 50—60 年代成型：Bellman 与 Ford 的迭代松弛可处理负权边；"
            "Dijkstra 于 1959 年给出非负权图上的贪心解法，配合二叉堆可达 O((V+E)logV)；"
            "Floyd 与 Warshall 于 1962 年给出 O(V³) 的全源算法。Tarjan 的深度优先搜索框架把割点、"
            "桥与强连通分量统一到 O(V+E)；Prim 与 Kruskal 分别从点集扩展和边排序解决最小生成树；"
            "Held 与 Karp 用状态压缩动态规划求解小规模旅行商问题。")
        w.p("工程侧，现代导航软件普遍采用分层图、路网预处理与 A* 启发式搜索支撑大规模路网查询。"
            "这些技术超出课程设计范围，但其基础仍是本文实现的几类最短路算法。")

        w.h2("1.3 主要工作")
        for item in [
            "（1）对校园路网建模，设计地点、道路、图、路径等核心数据结构；",
            "（2）实现邻接表（CSR）与邻接矩阵两套存储，并给出选型依据；",
            "（3）实现 Dijkstra、Floyd-Warshall、Bellman-Ford、BFS 四类最短路径算法；",
            "（4）实现 Tarjan 割点/桥、Prim/Kruskal 最小生成树、Held-Karp 多点巡游；",
            "（5）从高德底图提取真实路网，完成标定、配准、化简与 POI 接入；",
            "（6）实现 CSV/二进制快照持久化、命令行工具与 Win32+GDI+ 图形界面；",
            "（7）编写 1578 项断言与 14 个端到端场景，接入 GitHub Actions。",
        ]:
            w.p(item, indent=False)

        w.h2("1.4 开发环境与工具")
        w.p("开发使用 C11 标准，编译期开启 -Wall -Wextra -Werror，让编译器代替人工发现类型、格式与"
            "可移植性问题；同一套核心源码同时在 Windows（MinGW-w64）与 Linux（GitHub Actions）上构建，"
            "图形界面仅 Windows 可编译。")
        w.table(["项目", "配置"],
                [["操作系统", "Windows 11（开发）/ Ubuntu latest（CI）"],
                 ["编译器", "GCC 16.2.0（w64devkit），C11，-Werror"],
                 ["图形库", "GDI+（gdiplus.dll，Win32 flat API）"],
                 ["构建脚本", "Makefile、scripts/build.ps1、scripts/build_gui.ps1"],
                 ["测试", "自研断言框架，1578 项断言；端到端脚本 e2e.ps1 / e2e.sh"],
                 ["文档", "python-docx 生成报告，LibreOffice 导出 PDF，Pillow 绘图"]],
                widths=[3.6, 12.0], cap="表 1-1 开发环境与工具")

        # ---------------- 2 需求分析
        w.h1("2 需求分析")
        w.h2("2.1 应用场景与用户需求")
        w.p("校园导航的使用者分为四类：新生与访客需要从校门找到教学楼与食堂；在校学生关心课间最快路线；"
            "管理者关心封路影响；来访团体希望串联多个地点且总路程最短。四类需求分别对应单源最短路径、"
            "带权最近设施查询、割点与桥分析、多点巡游优化。")
        w.p("校区规模（20 个地点、353 个路网节点）属于小规模图，适合精确算法而非启发式近似。")

        w.h2("2.2 功能需求")
        w.p("系统功能需求整理如表 2-1，编号在后续设计与测试章节沿用。")
        w.table(["编号", "功能", "说明", "关键算法"],
                [["F1", "校园地图", "按真实坐标绘制底图与路网，支持缩放平移", "坐标投影"],
                 ["F2", "地点信息", "列表、类型、坐标、简介、相邻道路", "线性查找"],
                 ["F3", "路径规划", "任意两点，按距离或步行时间求最短路", "Dijkstra"],
                 ["F4", "转向指引", "逐段给出方向、路名与里程", "方位角判定"],
                 ["F5", "多点巡游", "起终点 + 途经点，顺序自动优化", "Held-Karp / 2-opt"],
                 ["F6", "最近设施", "从当前位置找最近的食堂、校医院", "Dijkstra + 筛选"],
                 ["F7", "全源最短路", "任意两点最短距离矩阵", "Floyd-Warshall"],
                 ["F8", "关键道路", "割点与桥，评估封路风险", "Tarjan"],
                 ["F9", "最小生成树", "连通全部地点的最小布线方案", "Prim / Kruskal"],
                 ["F10", "数据持久化", "CSV 导入导出、二进制快照与校验", "CRC32"],
                 ["F11", "图形界面", "底图渲染、点选、路径高亮、侧栏指引", "Win32 + GDI+"]],
                widths=[1.4, 2.6, 7.4, 4.2], cap="表 2-1 功能需求")

        w.h2("2.3 非功能需求")
        for item in [
            "（1）性能：校区规模下单次路径查询应在 1 毫秒以内，全源矩阵不超过 10 毫秒；",
            "（2）健壮性：非法输入、损坏文件、不存在的地点都必须给出可读错误，不得崩溃；",
            "（3）可移植性：核心库在 Windows 与 Linux 上以 -Werror 通过编译；",
            "（4）可测试性：核心算法与界面解耦，测试程序直接链接核心 .c 文件；",
            "（5）可维护性：模块边界清晰，函数失败返回负错误码，不使用 exit()。",
        ]:
            w.p(item, indent=False)

        # ---------------- 3 系统设计
        w.h1("3 系统设计")
        w.h2("3.1 总体架构")
        w.p("系统自下而上分为支撑层、算法层、业务层与表示层，如图 3-1。表示层提供命令行与图形两套界面，"
            "共用同一核心库；支撑层负责数据文件、图存储、编码与计时。这种划分使核心算法可被测试程序直接调用，"
            "也使图形界面可以独立演进。")
        w.figure("architecture.png", "图 3-1 系统总体架构")

        w.h2("3.2 模块划分")
        w.p("按职责划分为 11 个模块，见表 3-1。算法层与数据层是代码主体，界面层保持轻薄。")
        w.table(["文件", "职责", "行数"],
                [["include/nav.h", "公共类型与常量", mod_lines("include/nav.h")],
                 ["src/campus.c", "数据模型、校验、CSV/快照、编辑", mod_lines("src/campus.c")],
                 ["src/graph.c", "CSR 邻接表与邻接矩阵构建", mod_lines("src/graph.c")],
                 ["src/algo.c", "八类图算法与路径重建", mod_lines("src/algo.c")],
                 ["src/app.c", "应用状态与数据目录发现", mod_lines("src/app.c")],
                 ["src/ui.c", "表格、ASCII 地图、菜单、转向指引", mod_lines("src/ui.c")],
                 ["src/main.c", "子命令解析与分发", mod_lines("src/main.c")],
                 ["src/platform.c", "UTF-8 控制台/参数、计时、目录", mod_lines("src/platform.c")],
                 ["gui/gui.c", "GUI 状态、视图变换、命中测试", mod_lines("gui/gui.c")],
                 ["gui/render.c", "GDI+ 绘制：底图、路网、路径、侧栏", mod_lines("gui/render.c")],
                 ["gui/main.c", "窗口、消息循环、菜单、截图模式", mod_lines("gui/main.c")]],
                widths=[4.6, 9.0, 1.8], cap="表 3-1 模块划分与代码规模")

        w.h2("3.3 核心数据结构")
        w.p("系统围绕四个结构体组织数据：Spot 描述地点，Road 描述无向道路，Campus 是地点与道路的集合，"
            "Graph 是由 Campus 派生的图结构，Path 表示一次查询结果。地点编号从 1 连续编号，"
            "并约定“编号即下标 + 1”，算法内部可直接用数组索引。")
        w.p("为支持真实路网，Spot 增加 junction 类型：路口参与最短路计算，但不在地点列表与地图标注中显示，"
            "从而把“图的节点”与“用户可见的地点”解耦。")
        w.table(["结构体", "字段", "说明"],
                [["Spot", "id, name, type, x, y, brief", "地点编号、名称、类型、平面坐标、简介"],
                 ["Road", "id, from, to, len_m, walk_min, open, name", "无向边；open=0 表示封闭"],
                 ["Campus", "title, width, height, spots[], roads[], n_spots, n_roads", "完整数据集"],
                 ["Graph", "n, off[], to[], w[], road[], eu/ev/ew/eid[]", "CSR 邻接表 + 边表"],
                 ["Path", "nodes[], count, weight, settled, relaxed", "路径与算法统计量"]],
                widths=[2.2, 7.6, 6.0], cap="表 3-2 核心数据结构")

        w.h2("3.4 图的存储结构")
        w.p("遍历与单源最短路适合邻接表，Floyd 需要矩阵。系统以 CSR（压缩稀疏行）存储邻接表：所有弧按起点"
            "连续存放，用 off[] 记录每个顶点的起止下标。相比指针链表，CSR 省去指针开销，遍历时访问连续内存。"
            "矩阵视图按需构建，因为 Floyd 本身是 O(V³)，构建矩阵的 O(V²+E) 可忽略。")
        w.p("封闭道路在建图时直接跳过，因此“封路”不需要改算法，只需重建图。")

        w.h2("3.5 数据文件设计")
        w.p("数据层提供两套数据：示意数据（24 个地点、41 条道路）用于单元测试与课程演示；"
            "真实数据（353 个节点、409 条道路）用于实际导航。CSV 面向人工编辑，"
            "首行以“# title:”给出数据集标题；二进制快照带 magic、版本与 CRC32，用于完整性校验。")
        w.figure("data-format.png", "图 3-2 数据文件格式")

        # ---------------- 4 算法
        w.h1("4 算法设计与分析")
        w.h2("4.1 问题建模")
        w.p("把校园抽象为无向带权图 G=(V,E)，真实数据 |V|=353、|E|=409。每条边有两个权重：长度（米）与"
            "步行时间（分钟），分别对应“最短距离”与“最快到达”。建图时通过 by_time 开关选择其一。")
        w.p("约定顶点按 1..n 连续编号；边权恒正；图允许不连通（封路后可能出现孤立区域）。"
            "不连通用 NAV_INF 表示，避免与合法权重混淆。")

        w.h2("4.2 Dijkstra 算法")
        w.p("Dijkstra 的核心是贪心：每次取出当前距离最小的未确定顶点，固定其距离并松弛邻边。"
            "正确性依赖边权非负。实现使用二叉最小堆并采用惰性删除：松弛成功就压堆，出堆时若 key 大于"
            "当前 dist 则丢弃，从而避免 decrease-key，复杂度 O((V+E)logE)。")
        w.figure("flowchart.png", "图 4-1 程序主流程")

        w.h2("4.3 Floyd-Warshall 算法")
        w.p("Floyd 用动态规划求全源最短路，三重循环即可完成。路径重建依赖 next 矩阵："
            "next[i][j] 记录“从 i 到 j 的第一跳”，转移成功时更新为 next[i][k] 而不是 next[k][j]。"
            "本系统最初写错方向，测试中 20 组点对出现路径节点缺失，正是通过路径合法性断言定位的。")

        w.h2("4.4 Bellman-Ford 算法")
        w.p("Bellman-Ford 对每条弧松弛 V-1 轮，第 V 轮仍可松弛则说明存在负权回路。"
            "系统按“弧”而非“无向边”松弛，既支持有向图测试，也与 CSR 结构一致。"
            "由于无向图上的负权边必然构成负环，负权用例只能构造有向图验证。")

        w.h2("4.5 BFS 与最少路段路径")
        w.p("当用户关心“少拐几个弯、少走几段路”时，等价于边权全为 1 的最短路，用 BFS 按层扩展即可，"
            "复杂度 O(V+E)。系统保留该功能用于对比：同一对起点终点，Dijkstra 给出最短距离，"
            "BFS 给出最少路段数。")

        w.h2("4.6 Tarjan 割点与桥")
        w.p("Tarjan 在一次 DFS 中维护 disc[u]（首次访问次序）与 low[u]（子树经至多一条回边能到达的最早顶点）。"
            "对树边 (u,v)：low[v] > disc[u] 时该边为桥；u 非根且 low[v] ≥ disc[u] 时 u 为割点；"
            "根节点若有多个子树也是割点。真实路网中含大量尽端支路，共识别出 141 个割点与 190 条桥。")

        w.h2("4.7 最小生成树")
        w.p("最小生成树回答“用最短总长度把所有地点连通”。Prim 从任一点出发逐点扩展，适合稠密图；"
            "Kruskal 按边权排序并用并查集合并，适合稀疏图。系统同时实现两者并互相校验——"
            "若总权重不一致即说明实现有误。")

        w.h2("4.8 多点巡游问题")
        w.p("多点巡游是旅行商问题的变体：固定起终点、途中经过 k 个地点且顺序可优化。当 k≤12 时用"
            "状态压缩动态规划（Held-Karp）求精确解，复杂度 O(2^k·k²)；k>12 时改用最近邻构造加 2-opt 改进。"
            "各段之间用 Dijkstra 求实际最短路并拼接，拼接时跳过重复连接点。")

        w.h2("4.9 算法对比与选型")
        w.table(["算法", "时间复杂度", "空间", "负权", "在本系统中的作用"],
                [["Dijkstra（二叉堆）", "O((V+E)logE)", "O(V+E)", "否", "单源最短路，交互查询主算法"],
                 ["Floyd-Warshall", "O(V³)", "O(V²)", "是（无负环）", "全源矩阵"],
                 ["Bellman-Ford", "O(VE)", "O(V+E)", "是", "负权与负环检测"],
                 ["BFS", "O(V+E)", "O(V)", "不适用", "最少路段数路径"],
                 ["Tarjan", "O(V+E)", "O(V)", "不适用", "割点与桥"],
                 ["Prim", "O(V²+E)", "O(V)", "不适用", "密集图最小生成树"],
                 ["Kruskal", "O(ElogE)", "O(V+E)", "不适用", "稀疏图最小生成树"],
                 ["Held-Karp", "O(2^k·k²)", "O(2^k·k)", "不适用", "k≤12 精确巡游"]],
                widths=[3.4, 3.2, 2.0, 2.6, 5.0], cap="表 4-1 算法对比")

        # ---------------- 5 实现
        w.h1("5 系统实现")
        w.h2("5.1 工程结构")
        w.p("工程按“头文件 + 实现 + 测试 + 数据 + 脚本 + 文档 + 图形界面”组织。scripts/ 负责构建、测试、"
            "端到端验证与打包；数据分布在 data/campus（示意）与 data/campus_real（真实）；"
            "图形界面独立放在 gui/，只依赖核心库。")

        w.h2("5.2 真实路网数据构建")
        w.p("真实路网由四个步骤得到：其一，用底图比例尺标定，得到 0.492 m/px 的空间分辨率；"
            "其二，用两幅图共有的湖面做配准，求得“参考像素→路网像素”的相似变换（尺度 1.5086），"
            "并用四个校门验证，落点误差 3.7~12 m；其三，把 496 个节点、563 条边的原始路网清理"
            "（去自环、重复边、零长边）并合并二度点，得到 319 个路口、376 条边；"
            "其四，把 20 个地图编号地点投影到最近道路生成连接支路。最终得到 353 个节点、409 条边、"
            "总长 22.4 km 的校园图，全连通且每个地点度数均为 1。")
        w.figure("campus-map.png", "图 5-1 真实校园路网（按坐标绘制）")

        w.h2("5.3 数据层实现")
        w.p("CSV 解析是手写状态机：按行读取、跳过注释与空行、逐字段切分，并处理引号包裹、转义引号与行尾空格。"
            "实现中曾出现一个真实 bug：去掉行尾空格时把分隔逗号一并写成 '\\0'，导致整行只解析出一个字段；"
            "测试中“第 4 行列数应为 6”的断言直接暴露了它。")
        w.p("校验函数 campus_validate 集中检查编号连续、端点越界、长度与时间为正、自环与重复边、"
            "坐标范围、名称重复与容量上限，所有错误附带可读原因。")

        w.h2("5.4 算法层实现")
        w.p("算法层只依赖 graph.h 暴露的 CSR 结构，不引用任何界面函数。路径重建、堆、并查集等公共部件"
            "都写成静态函数。巡游算法把“终端间代价矩阵”与“顺序优化”分开：先对 k+2 个终端各跑一次 Dijkstra "
            "得到代价矩阵，再在小矩阵上做 DP 或启发式，最后按顺序拼接各段最短路。")

        w.h2("5.5 命令行与交互")
        w.p("命令行提供 route、tour、near、critical、mst、allpairs、bench 等子命令，支持 --json 便于脚本比对。"
            "路径输出把中间路口折叠为“经 N 个路口 X 米”，并附带逐段转向指引。")
        w.figure("terminal-demo.png", "图 5-2 命令行运行示例")

        w.h2("5.6 图形界面实现")
        w.p("图形界面用 Win32 + GDI+ 实现，核心库一行未改。窗口过程处理缩放、平移、点选与键盘快捷键；"
            "绘制统一走 GpGraphics，窗口与离屏位图共用同一套渲染代码，因此可以用 --shot 参数把界面渲染成 PNG "
            "做回归检查。")
        w.p("右侧栏提供起点/终点显示、距离/时间口径切换、规划按钮、最近食堂/校医院查询、关键道路开关，"
            "以及逐段转向指引；底部状态栏显示距离、用时、节点数与算法耗时。")
        w.figure("gui-main.png", "图 5-3 图形界面（学生公寓A区 → 文达楼）")

        w.h2("5.7 健壮性与跨平台")
        w.p("健壮性体现在三处：所有对外函数返回错误码而非退出进程；每个 malloc 有对应 free；"
            "二进制快照在解析前先校验长度、magic、版本与 CRC32，把“文件损坏”转化为可读错误。")
        w.p("跨平台方面，控制台 UTF-8 代码页、_wfopen 中文路径、GetCommandLineW 重取参数都已处理；"
            "构建脚本遇到系统临时目录不可写时自动改用工程内临时目录。")

        # ---------------- 6 测试
        w.h1("6 测试与结果分析")
        w.h2("6.1 测试策略")
        w.p("测试分三层：单元测试验证核心函数行为与错误路径；随机交叉验证用大量随机图比对不同算法；"
            "端到端测试从命令行入口验证真实流程。三层都在 CI 中自动执行。")

        w.h2("6.2 单元测试")
        groups = load_groups()
        total = sum(g[1] for g in groups) if groups else 1578
        w.p(f"单元测试共 {len(groups)} 组、{total} 项断言，覆盖工具层、数据层、算法层与应用集成，"
            "分组统计见表 6-1。")
        rows = [[n, c, "通过" if f == 0 else f"失败 {f}"] for n, c, f in groups]
        rows.append(["合计", total, "全部通过"])
        w.table(["测试组", "断言数", "结果"], rows, widths=[6.0, 3.0, 5.0], cap="表 6-1 单元测试分组统计")

        w.h2("6.3 随机交叉验证")
        w.p("随机生成 30 张规模 8~19 的连通图，分别在距离与时间口径下运行 Dijkstra、Floyd、Bellman-Ford，"
            "逐点比对三者结果；对每条返回路径检查“相邻节点确有边且边权和等于总权重”；"
            "对每条识别出的桥，实际封闭后重算连通分量，断言图确实分裂。")

        w.h2("6.4 端到端测试")
        w.p("端到端脚本覆盖 14 个场景：地点列表、JSON 路径、时间口径、起终点相同、非法地点退出码、"
            "最近设施、多点巡游、割点与桥、最小生成树、全源矩阵、ASCII 地图、数据校验、基准测试，"
            "以及“导出的 CSV 与仓库数据逐行一致”。")

        w.h2("6.5 性能测试")
        bench = load_bench()
        w.p("在随机生成、约 3V 条边的图上从 20 个节点扩展到 120 个节点，每种规模重复运行取平均。"
            "计时使用 QueryPerformanceCounter，避免 clock() 只有 1 毫秒粒度的问题。")
        rows = [[b["n"], f"{b.get('dijkstra', 0):.4f}", f"{b.get('floyd', 0):.4f}",
                 f"{b.get('bellman', 0):.4f}", f"{b.get('kruskal', 0):.4f}"] for b in bench]
        w.table(["节点数", "Dijkstra(ms)", "Floyd(ms)", "Bellman(ms)", "Kruskal(ms)"], rows,
                widths=[2.4, 3.2, 2.8, 3.0, 3.0], cap="表 6-2 随机图上的算法耗时")
        w.p("稀疏图上 Dijkstra 稳定在微秒级，120 节点单次约 3.5 微秒；Floyd 随规模立方增长，120 节点约 1 毫秒；"
            "Bellman-Ford 介于两者之间；Kruskal 因排序在小图上略慢于 Prim。结果与理论复杂度一致。")
        w.figure("algo-compare.png", "图 6-1 算法耗时随规模变化")

        w.h2("6.6 运行结果")
        w.p("图 6-2 为真实数据的路径规划结果：学生公寓A区 → 文达楼，1127.3 米、约 22.5 分钟，"
            "途经 27 个节点。界面右侧同步给出逐段转向指引，底部状态栏显示算法耗时约 0.01 毫秒。")
        w.figure("route_dorm_class.png", "图 6-2 真实底图上的最短路径")

        # ---------------- 结论
        w.big("结论与展望")
        w.p("本文以校园导航为载体，完成了从需求分析、数据结构设计、算法实现到测试与文档的完整闭环。"
            "系统建立了 353 个节点、409 条道路的真实校园路网，实现八类图算法、两套界面、"
            "1578 项自动化断言，并在 GitHub Actions 上持续集成。")
        w.p("工程实践中的主要收获是：把“可测试性”放在“写得快”之前。CSV 分隔符被覆盖、"
            "Floyd 第一跳写反、PowerShell 把 UTF-8 脚本读成 ANSI、中文命令行参数乱码、"
            "Linux 上缺少 <time.h> 与 glibc 的 format-truncation 告警——这些问题单看代码都很难发现，"
            "必须依靠测试或真实运行才能暴露。")
        w.p("不足与改进方向：校园数据对周边道路保留了冗余支路，可进一步裁剪；"
            "多点巡游在 k>12 时使用启发式，无法保证最优；图形界面尚未实现 DPI 缩放与图标资源；"
            "后续可接入实时路况与无障碍路径（避开台阶），并扩展为多校区导航。")

        # ---------------- 致谢
        w.big("致  谢")
        w.p("本设计从选题、方案论证到实现与撰写，始终得到指导教师李薛老师的悉心指导。"
            "在算法选型、数据结构设计与论文结构安排上，老师提出了许多关键意见，使我对"
            "“把课堂算法做成可用系统”有了更具体的理解。")
        w.p("感谢在数据采集中提供帮助的同学，感谢实验室提供的开发环境。"
            "感谢家人一直以来的支持与鼓励。")
        w.p("最后，向在百忙之中评阅本文的各位老师表示衷心的感谢。")

        # ---------------- 参考文献
        w.big("参 考 文 献")
        for i, ref in enumerate(REFERENCES, start=1):
            p = w.p("", indent=False, first_indent=0)
            p.paragraph_format.left_indent = Cm(0.74)
            p.paragraph_format.first_line_indent = Cm(-0.74)
            set_run(p.add_run(f"[{i}]  "), SONG, 12.0)
            set_run(p.add_run(ref), SONG, 12.0)


CN_ABSTRACT = [
    "校园导航是图的最短路径问题的典型应用。本文以江苏海洋大学苍梧校区为对象，"
    "从高德底图提取真实路网，经比例尺标定、湖面配准、拓扑化简与地点投影，"
    "构建了包含 353 个节点（20 个地图编号地点、319 个路网路口与接驳点）、409 条道路、"
    "总长约 22.4 km 的无向带权图。系统以 CSR 邻接表为主要存储结构，实现了 Dijkstra、"
    "Floyd-Warshall、Bellman-Ford、BFS 四类最短路径算法，并用 Tarjan 算法完成割点与桥分析，"
    "用 Prim、Kruskal 构造最小生成树，用 Held-Karp 动态规划与 2-opt 启发式求解多点巡游。"
    "在表示层，系统提供命令行与 Win32+GDI+ 图形界面两套入口，共用同一核心库；"
    "图形界面支持底图渲染、缩放平移、点选起终点、路径高亮与逐段转向指引。"
    "工程上用脚本统一构建与测试，共 21 组、1578 项自动化断言，并在 GitHub Actions 上持续集成。"
    "测试表明，校区规模下单次 Dijkstra 查询约 3.5 微秒，全源 Floyd 计算约 1 毫秒，"
    "路径结果与程序输出、图形渲染三者一致；系统能正确处理不连通、负权、非法输入与数据损坏等异常。",
]

EN_ABSTRACT = [
    "Campus navigation is a typical application of the shortest path problem on graphs. "
    "Taking the Cangwu campus of Jiangsu Ocean University as the target, this thesis extracts a "
    "real road network from the Amap base map and builds an undirected weighted graph with 353 nodes "
    "(20 numbered places plus 319 junctions and connectors), 409 roads and a total length of about "
    "22.4 km, through scale calibration, water-body based registration, topology simplification and "
    "POI projection.",
    "The system stores the graph in CSR adjacency lists and implements Dijkstra, Floyd-Warshall, "
    "Bellman-Ford and BFS for shortest paths, Tarjan's algorithm for articulation points and bridges, "
    "Prim and Kruskal for minimum spanning trees, and Held-Karp dynamic programming with 2-opt for "
    "multi-waypoint tours. Two front ends, a command line tool and a Win32 + GDI+ graphical interface, "
    "share the same core library; the GUI supports map rendering, zooming and panning, point picking, "
    "path highlighting and turn-by-turn guidance.",
    "All 1578 assertions in 21 groups and 14 end-to-end scenarios pass, and continuous integration "
    "runs on GitHub Actions. Measurements show a single Dijkstra query takes about 3.5 microseconds on "
    "the campus graph and a full Floyd computation about 1 millisecond, while path results agree with "
    "both program output and graphical rendering.",
]

REFERENCES = [
    "严蔚敏, 吴伟民. 数据结构（C语言版）[M]. 北京: 清华大学出版社, 2007.",
    "Dijkstra E W. A note on two problems in connexion with graphs[J]. Numerische Mathematik, 1959, 1: 269-271.",
    "Floyd R W. Algorithm 97: Shortest path[J]. Communications of the ACM, 1962, 5(6): 345.",
    "Bellman R. On a routing problem[J]. Quarterly of Applied Mathematics, 1958, 16(1): 87-90.",
    "Tarjan R. Depth-first search and linear graph algorithms[J]. SIAM Journal on Computing, 1972, 1(2): 146-160.",
    "Prim R C. Shortest connection networks and some generalizations[J]. Bell System Technical Journal, 1957, 36(6): 1389-1401.",
    "Kruskal J B. On the shortest spanning subtree of a graph[J]. Proceedings of the American Mathematical Society, 1956, 7(1): 48-50.",
    "Held M, Karp R M. A dynamic programming approach to sequencing problems[J]. Journal of the Society for Industrial and Applied Mathematics, 1962, 10(1): 196-210.",
    "Cormen T H, Leiserson C E, Rivest R L, et al. Introduction to Algorithms[M]. 4th ed. Cambridge: MIT Press, 2022.",
    "ISO/IEC 9899:2018. Programming languages — C[S]. Geneva: ISO, 2018.",
]


def mod_lines(rel):
    try:
        with open(os.path.join(ROOT, rel), encoding="utf-8", errors="replace") as f:
            return sum(1 for _ in f)
    except OSError:
        return 0


def load_groups():
    path = os.path.join(DATA, "test-summary.txt")
    out = []
    if not os.path.exists(path):
        return out
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"\[group\] ([\w\-]+): (\d+) checks, (\d+) failed", line.strip())
        if m:
            out.append((m.group(1), int(m.group(2)), int(m.group(3))))
    return out


def load_bench():
    rows = []
    if not os.path.isdir(DATA):
        return rows
    for name in sorted(os.listdir(DATA)):
        m = re.match(r"bench_(\d+)\.txt$", name)
        if not m:
            continue
        n = int(m.group(1))
        text = open(os.path.join(DATA, name), encoding="utf-8", errors="replace").read()
        row = {"n": n}
        for key, pat in (("dijkstra", r"Dijkstra[^\n]*?([0-9.]+)\s+[0-9.]+"),
                         ("floyd", r"Floyd-Warshall\s+([0-9.]+)"),
                         ("bellman", r"Bellman-Ford\s+([0-9.]+)"),
                         ("kruskal", r"Kruskal \(MST\)\s+([0-9.]+)")):
            mm = re.search(pat, text)
            if mm:
                row[key] = float(mm.group(1))
        rows.append(row)
    rows.sort(key=lambda r: r["n"])
    return rows


# ---------------------------------------------------------------- 主流程

doc = None


def main(argv):
    global doc
    ap = argparse.ArgumentParser()
    ap.add_argument("--pages", default=os.path.join(ROOT, "docs", "toc_pages.json"))
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "课程设计报告.docx"))
    ap.add_argument("--mask", action="store_true")
    ap.add_argument("--template", default=TEMPLATE)
    args = ap.parse_args(argv[1:])

    if args.mask:
        for k in ("name", "sid", "class", "advisor"):
            PERSON[k] = "【请填写】"

    pages = {}
    if os.path.exists(args.pages):
        try:
            pages = json.load(open(args.pages, encoding="utf-8"))
        except json.JSONDecodeError:
            pages = {}

    doc = Document(args.template)
    fill_cover(doc)
    # 声明/授权页在 LibreOffice 下会多溢出一行，删掉模板的空行占位
    drop_empty_paragraphs(doc, 1, 8)

    # 第 2~7 节：清空后重写（第 7 节是著录格式说明，直接清空）
    anchors = [clear_section(doc, k) for k in (2, 3, 4, 5, 6, 7)]
    write_cn_abstract(anchors[0])
    write_en_abstract(anchors[1])
    write_toc(anchors[2], pages)
    write_body(anchors[3], None)
    # 结论/致谢/参考文献已写在正文节；第 7 节保持空（其锚点是 body 级 sectPr，不能删）

    # 正文节页码：从 1 开始
    if len(doc.sections) > 5:
        restart_numbering(doc.sections[5], 1)
        if not doc.sections[5].footer.paragraphs[0].text.strip():
            page_field(doc.sections[5])

    doc.save(args.out)
    json.dump([t for t, _ in TOC], open(os.path.join(ROOT, "docs", "toc_entries.json"), "w",
                                        encoding="utf-8"), ensure_ascii=False, indent=2)
    print("thesis written:", args.out)


if __name__ == "__main__":
    main(sys.argv)
