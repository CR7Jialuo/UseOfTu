#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 12 页答辩幻灯片（python-pptx）。

风格：16:9，深蓝标题 + 细装饰线，正文要点式；插图直接引用 docs/diagrams 下的 PNG。
"""

import os
import sys

from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.util import Emu, Inches, Pt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FIG = os.path.join(ROOT, "docs", "diagrams")

NAVY = RGBColor(0x1F, 0x3A, 0x5F)
BLUE = RGBColor(0x25, 0x63, 0xEB)
AMBER = RGBColor(0xF5, 0x9E, 0x0B)
GRAY = RGBColor(0x4B, 0x55, 0x63)
LIGHT = RGBColor(0xEE, 0xF2, 0xF7)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)

FONT = "微软雅黑"

W, H = Inches(13.333), Inches(7.5)

PERSON = {"class": "信息1241", "name": "宋家洛", "sid": "2024122827", "advisor": "李薛"}


def apply_mask():
    for k in PERSON:
        PERSON[k] = "【请填写】"


def blank(prs):
    return prs.slides.add_slide(prs.slide_layouts[6])


def textbox(slide, left, top, width, height, text, size=20, bold=False, color=GRAY,
            align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP, spacing=1.15):
    box = slide.shapes.add_textbox(left, top, width, height)
    tf = box.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    lines = text.split("\n")
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.line_spacing = spacing
        run = p.add_run()
        run.text = line
        run.font.size = Pt(size)
        run.font.bold = bold
        run.font.color.rgb = color
        run.font.name = FONT
    return box


def rect(slide, left, top, width, height, color, line=False):
    from pptx.enum.shapes import MSO_SHAPE
    shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, left, top, width, height)
    shape.fill.solid()
    shape.fill.fore_color.rgb = color
    if line:
        shape.line.color.rgb = color
    else:
        shape.line.fill.background()
    shape.shadow.inherit = False
    return shape


def header(slide, title, subtitle=None):
    rect(slide, 0, 0, W, Inches(0.12), NAVY)
    textbox(slide, Inches(0.9), Inches(0.45), Inches(11.5), Inches(0.7), title, 30, True, NAVY)
    rect(slide, Inches(0.9), Inches(1.2), Inches(1.4), Inches(0.06), AMBER)
    if subtitle:
        textbox(slide, Inches(0.9), Inches(1.35), Inches(11.5), Inches(0.5), subtitle, 15, False, GRAY)


def footer(slide, idx, total):
    textbox(slide, Inches(0.9), Inches(6.95), Inches(7.0), Inches(0.35),
            "基于最短路径算法的校园导航系统设计与实现", 10, False, RGBColor(0x9C, 0xA3, 0xAF))
    textbox(slide, Inches(11.4), Inches(6.95), Inches(1.0), Inches(0.35), f"{idx} / {total}",
            10, False, RGBColor(0x9C, 0xA3, 0xAF), PP_ALIGN.RIGHT)


def bullets(slide, items, left=Inches(0.9), top=Inches(1.9), width=Inches(11.6),
            size=19, gap=Pt(10), color=GRAY):
    box = slide.shapes.add_textbox(left, top, width, Inches(4.6))
    tf = box.text_frame
    tf.word_wrap = True
    for i, item in enumerate(items):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.space_after = gap
        p.line_spacing = 1.25
        if isinstance(item, tuple):
            text, bold = item
        else:
            text, bold = item, False
        run = p.add_run()
        run.text = text
        run.font.size = Pt(size)
        run.font.bold = bold
        run.font.color.rgb = NAVY if bold else color
        run.font.name = FONT
    return box


def picture(slide, name, max_w=Inches(11.6), max_h=Inches(4.9), top=Inches(1.75),
            crop_top=0):
    path = os.path.join(FIG, name)
    if not os.path.exists(path):
        textbox(slide, Inches(0.9), top, Inches(8), Inches(0.6), f"[缺少图片 {name}]", 16, False,
                RGBColor(0xC0, 0x00, 0x00))
        return
    from PIL import Image
    with Image.open(path) as im:
        iw, ih = im.size
        if crop_top > 0:
            # 去掉图片自带的标题，避免与幻灯片标题重复
            im = im.crop((0, crop_top, iw, ih))
            iw, ih = im.size
            tmp = os.path.join(FIG, "_slide_" + name)
            im.save(tmp)
            path = tmp
    ratio = min(max_w / iw, max_h / ih)
    w, h = int(iw * ratio), int(ih * ratio)
    left = int((W - w) / 2)
    slide.shapes.add_picture(path, left, int(top), width=w, height=h)


def table_like(slide, rows, left=Inches(0.9), top=Inches(1.95), width=Inches(11.6),
               col_ratios=(0.28, 0.44, 0.28), size=15):
    total = sum(col_ratios)
    widths = [int(width * r / total) for r in col_ratios]
    y = int(top)
    row_h = Inches(0.46)
    for ri, row in enumerate(rows):
        x = int(left)
        for ci, cell in enumerate(row):
            fill = NAVY if ri == 0 else (LIGHT if ri % 2 == 1 else WHITE)
            color = WHITE if ri == 0 else GRAY
            rect(slide, x, y, widths[ci], row_h, fill, line=False)
            textbox(slide, x + Inches(0.12), y + Inches(0.06), widths[ci] - Inches(0.24),
                    row_h - Inches(0.1), str(cell), size, ri == 0, color,
                    MSO_ANCHOR.MIDDLE)
            x += widths[ci]
        y += row_h


def main(argv):
    global args_out
    args_out = None
    argv = list(argv)
    if "--out" in argv:
        i = argv.index("--out")
        args_out = argv[i + 1]
        del argv[i:i + 2]
    if "--mask" in argv:
        apply_mask()
        argv.remove("--mask")
    prs = Presentation()
    prs.slide_width = W
    prs.slide_height = H
    total = 12
    idx = 0

    # 1 封面
    idx += 1
    s = blank(prs)
    rect(s, 0, 0, W, H, NAVY)
    rect(s, Inches(0.9), Inches(2.05), Inches(1.6), Inches(0.08), AMBER)
    textbox(s, Inches(0.9), Inches(2.35), Inches(11.5), Inches(1.0),
            "基于最短路径算法的校园导航系统设计与实现", 36, True, WHITE)
    textbox(s, Inches(0.9), Inches(3.45), Inches(11.5), Inches(0.6),
            "本科毕业设计（论文）· C11 + Win32/GDI+ · 江苏海洋大学苍梧校区", 18, False,
            RGBColor(0xC7, 0xD2, 0xE0))
    textbox(s, Inches(0.9), Inches(5.4), Inches(11.5), Inches(1.2),
            PERSON["class"] + "    " + PERSON["name"] + "    " + PERSON["sid"] +
            "\n指导教师：" + PERSON["advisor"], 16, False,
            RGBColor(0xC7, 0xD2, 0xE0))

    # 2 选题背景
    idx += 1
    s = blank(prs)
    header(s, "选题背景：校园导航就是一张图")
    bullets(s, [
        ("问题：353 个路网节点、409 条道路、22.4 km，从宿舍到教学楼怎么走最近？", True),
        "建模：地点 = 顶点，道路 = 边，长度/步行时间 = 边权 → 无向带权图",
        "单源最短路：任意两地的最快路线（Dijkstra）",
        "全源最短路：任意两地距离一览（Floyd-Warshall）",
        "关键道路：哪条路封了会让部分区域不可达（割点与桥）",
        "布线方案：连通全部地点的最小总长（最小生成树）",
        ("目标：算法讲得清、代码写得对、结果测得出、数据可替换", True),
    ])
    footer(s, idx, total)

    # 3 需求与功能
    idx += 1
    s = blank(prs)
    header(s, "需求分析：11 项功能，两类用户入口")
    table_like(s, [
        ["功能", "说明", "算法"],
        ["路径规划", "任取两地，可切换距离/时间口径", "Dijkstra"],
        ["多点巡游", "起终点 + 途经点，顺序自动优化", "Held-Karp / 2-opt"],
        ["最近设施", "找最近的食堂、医院等", "Dijkstra + 筛选"],
        ["全源矩阵", "任意两地最短距离表", "Floyd-Warshall"],
        ["关键道路", "割点与桥，评估封路风险", "Tarjan"],
        ["最小生成树", "校园道路布线方案", "Prim / Kruskal"],
        ["编辑与持久化", "增删改、CSV 与快照", "CRC32"],
    ], size=14)
    textbox(s, Inches(0.9), Inches(6.35), Inches(11.6), Inches(0.5),
            "两条入口共用同一套核心接口：交互菜单（演示）+ 子命令（脚本与测试）", 14, False, BLUE)
    footer(s, idx, total)

    # 4 架构
    idx += 1
    s = blank(prs)
    header(s, "系统设计：四层架构，核心库与界面解耦")
    picture(s, "architecture.png", max_h=Inches(4.7), top=Inches(1.82), crop_top=140)
    footer(s, idx, total)

    # 5 数据结构与存储
    idx += 1
    s = blank(prs)
    header(s, "数据结构：一套数据，两种图视图")
    bullets(s, [
        ("Spot / Road → Campus → Graph → Path", True),
        "邻接表用 CSR：弧按起点连续存放，遍历缓存友好，无指针开销",
        "Floyd 需要矩阵：按需构建 O(V²+E)，与 O(V³) 的算法本身相比可忽略",
        "封闭道路在建图时跳过 → 封路不需要改算法，只需重建图",
        "数据由 CSV 驱动，替换文件即可换校区；二进制快照带 magic + 版本 + CRC32",
        "核心库不依赖控制台，测试程序直接链接 .c 文件",
    ])
    footer(s, idx, total)

    # 6 算法 1
    idx += 1
    s = blank(prs)
    header(s, "核心算法（一）：单源与全源最短路")
    table_like(s, [
        ["算法", "复杂度", "作用"],
        ["Dijkstra + 二叉堆", "O((V+E)logE)", "交互查询主算法"],
        ["Floyd-Warshall", "O(V³)", "全源矩阵与路径重建"],
        ["Bellman-Ford", "O(VE)", "负权与负环检测"],
        ["BFS", "O(V+E)", "最少路段数路径"],
    ], col_ratios=(0.34, 0.30, 0.36), size=16)
    textbox(s, Inches(0.9), Inches(4.55), Inches(11.6), Inches(1.6),
            "实现要点：堆采用惰性删除，不做 decrease-key；路径用 prev/next 回溯；\n"
            "Floyd 的 next[i][j] 必须更新为 next[i][k]（第一跳），写错会导致路径缺节点。",
            15, False, GRAY)
    footer(s, idx, total)

    # 7 算法 2
    idx += 1
    s = blank(prs)
    header(s, "核心算法（二）：把图论用到校园管理")
    bullets(s, [
        ("Tarjan：一次 DFS 求出割点与桥", True),
        "本数据识别出 2 个割点、2 条桥（观海亭、实训码头为单通路设施）",
        "路径规划时若经过桥，自动提示“封路将导致绕行或断路”",
        ("Prim / Kruskal：两种最小生成树互相校验", True),
        "校园布线总长 3477.7 米，选中 23 条道路；两算法结果必须一致",
        ("Held-Karp：k≤12 精确解，k>12 用最近邻 + 2-opt", True),
        "各段用 Dijkstra 求最短路，拼接时去掉重复连接点",
    ])
    footer(s, idx, total)

    # 8 数据与工程
    idx += 1
    s = blank(prs)
    header(s, "真实路网：从底图到可导航的图", "0.492 m/px 标定 · 湖面配准 · 二度点合并 · POI 投影")
    picture(s, "campus-map.png", max_h=Inches(4.7), top=Inches(1.82), crop_top=100)
    footer(s, idx, total)

    # 9 演示
    idx += 1
    s = blank(prs)
    header(s, "图形界面：Win32 + GDI+", "缩放平移 · 点选起终点 · 路径高亮 · 逐段转向指引")
    picture(s, "gui-main.png", max_h=Inches(4.7), top=Inches(1.82), crop_top=0)
    footer(s, idx, total)

    # 10 测试
    idx += 1
    s = blank(prs)
    header(s, "测试：三层验证，全部自动化")
    bullets(s, [
        ("21 组、1578 项断言全部通过；14 个端到端场景全绿", True),
        "单元测试：数据层（CSV 往返、6 类错误、快照篡改/截断）、算法层、编辑与查找",
        "随机交叉验证：30 张随机图，Dijkstra / Floyd / Bellman-Ford 全点对一致",
        "性质测试：路径相邻必有边且权重求和一致；每条桥封闭后图确实分裂",
        "端到端：14 个场景，覆盖 JSON 输出、退出码与“导出数据与仓库一致”",
        "CI：GitHub Actions 在 Ubuntu 上 make → make test → make e2e 全绿",
    ])
    footer(s, idx, total)

    # 11 性能
    idx += 1
    s = blank(prs)
    header(s, "性能：毫秒以内完成交互查询")
    picture(s, "algo-compare.png", max_h=Inches(4.9), top=Inches(1.6), crop_top=130)
    footer(s, idx, total)

    # 12 总结
    idx += 1
    s = blank(prs)
    header(s, "总结与展望")
    bullets(s, [
        ("完成：11 项功能、8 类算法、双层入口、1578 项断言、CI 全绿", True),
        "收获：把课本算法做成可演示、可测试、可维护的完整系统",
        "踩坑：CSV 分隔符被覆盖、Floyd 第一跳写反、PS 脚本编码、中文参数乱码",
        ("不足：数据为教学示意值；地点上限 128；巡游大 k 用启发式", False),
        ("改进：接入真实测绘数据、图形化地图、多目标（少台阶、避雨）路径", True),
    ], size=20)
    footer(s, idx, total)

    out = args_out or os.path.join(ROOT, "docs", "答辩演示.pptx")
    prs.save(out)
    print("slides written:", out)


if __name__ == "__main__":
    main(sys.argv)
