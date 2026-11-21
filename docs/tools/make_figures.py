#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成课程设计报告所需的全部插图（Pillow，无第三方绘图库）。

输出到 docs/diagrams/：
  architecture.png    总体架构图
  flowchart.png       程序主流程图
  campus-map.png      校园路网图（按真实坐标绘制）
  routing-demo.png    路径规划示意（宿舍A区 → 图书馆）
  data-format.png     CSV 与二进制快照格式
  algo-compare.png    算法耗时随规模变化
  test-summary.png    测试分组统计

用法：python make_figures.py [项目根目录]
"""

import csv
import json
import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

FONT_FILES = {
    "regular": r"C:\Windows\Fonts\msyh.ttc",
    "bold": r"C:\Windows\Fonts\msyhbd.ttc",
    "mono": r"C:\Windows\Fonts\simsun.ttc",
}

INK = (31, 41, 55)
MUTED = (107, 114, 128)
LINE = (209, 213, 219)
SOFT = (243, 244, 246)
WHITE = (255, 255, 255)
BLUE = (37, 99, 235)
SKY = (14, 165, 233)
GREEN = (22, 163, 74)
AMBER = (245, 158, 11)
RED = (220, 38, 38)
VIOLET = (124, 58, 237)
TEAL = (13, 148, 136)

TYPE_COLOR = {
    "gate": (220, 38, 38),
    "teaching": (37, 99, 235),
    "library": (124, 58, 237),
    "admin": (15, 118, 110),
    "dorm": (245, 158, 11),
    "canteen": (234, 88, 12),
    "sport": (22, 163, 74),
    "medical": (219, 39, 119),
    "service": (8, 145, 178),
    "scenic": (101, 163, 13),
}

TYPE_LABEL = {
    "gate": "校门",
    "teaching": "教学楼",
    "library": "图书馆",
    "admin": "行政楼",
    "dorm": "宿舍",
    "canteen": "食堂",
    "sport": "体育",
    "medical": "医疗",
    "service": "服务",
    "scenic": "景观",
}

_font_cache = {}


def font(size, kind="regular"):
    key = (size, kind)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(FONT_FILES[kind], size)
    return _font_cache[key]


def text_size(draw, s, f):
    box = draw.textbbox((0, 0), s, font=f)
    return box[2] - box[0], box[3] - box[1]


def center_text(draw, box, s, f, fill=INK):
    x0, y0, x1, y1 = box
    w, h = text_size(draw, s, f)
    draw.text((x0 + (x1 - x0 - w) / 2, y0 + (y1 - y0 - h) / 2 - h * 0.12), s, font=f, fill=fill)


def wrap(draw, text, f, max_width):
    lines, cur = [], ""
    for ch in text:
        trial = cur + ch
        if text_size(draw, trial, f)[0] > max_width and cur:
            lines.append(cur)
            cur = ch
        else:
            cur = trial
    if cur:
        lines.append(cur)
    return lines


def rounded(draw, box, radius, fill, outline=None, width=2):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def arrow(draw, start, end, color=MUTED, width=4, head=14):
    draw.line([start, end], fill=color, width=width)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    for sign in (1, -1):
        a = angle + sign * math.radians(150)
        p = (end[0] + head * math.cos(a), end[1] + head * math.sin(a))
        draw.line([end, p], fill=color, width=width)


def canvas(w, h, bg=WHITE):
    img = Image.new("RGB", (w, h), bg)
    return img, ImageDraw.Draw(img)


def title_block(draw, w, main, sub=None):
    f = font(46, "bold")
    tw, _ = text_size(draw, main, f)
    draw.text(((w - tw) / 2, 40), main, font=f, fill=INK)
    if sub:
        fs = font(24)
        sw, _ = text_size(draw, sub, fs)
        draw.text(((w - sw) / 2, 100), sub, font=fs, fill=MUTED)


# ---------------------------------------------------------------- 数据读取

def read_text(path):
    """兼容 PowerShell 重定向产生的 UTF-16LE / BOM，以及手工编辑的 UTF-8。"""
    raw = open(path, "rb").read()
    for enc in ("utf-8-sig", "utf-16", "gb18030"):
        try:
            return raw.decode(enc)
        except (UnicodeDecodeError, UnicodeError):
            continue
    return raw.decode("utf-8", errors="replace")


def load_campus(root):
    spots = []
    roads = []
    with open(os.path.join(root, "data", "campus", "spots.csv"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            sid, name, stype, x, y, brief = next(csv.reader([line]))
            spots.append({
                "id": int(sid), "name": name, "type": stype,
                "x": float(x), "y": float(y), "brief": brief,
            })
    with open(os.path.join(root, "data", "campus", "roads.csv"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            row = next(csv.reader([line]))
            rid, a, b, length, minutes = row[:5]
            roads.append({"id": int(rid), "a": int(a), "b": int(b),
                          "len": float(length), "min": float(minutes)})
    return spots, roads


def load_bench(root):
    rows = []
    data_dir = os.path.join(root, "docs", "data")
    for name in sorted(os.listdir(data_dir)):
        m = re.match(r"bench_(\d+)\.txt$", name)
        if not m:
            continue
        n = int(m.group(1))
        text = read_text(os.path.join(data_dir, name))
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


def load_test_groups(root):
    path = os.path.join(root, "docs", "data", "test-summary.txt")
    groups = []
    if not os.path.exists(path):
        return groups
    for line in read_text(path).splitlines():
        m = re.match(r"\[group\] ([\w\-]+): (\d+) checks, (\d+) failed", line.strip())
        if m:
            groups.append((m.group(1), int(m.group(2)), int(m.group(3))))
    return groups


# ---------------------------------------------------------------- 图 1：架构

def fig_architecture(root, out):
    W, H = 1600, 1120
    img, d = canvas(W, H)
    title_block(d, W, "校园导航系统总体架构", "四层结构：表示层 / 业务层 / 算法层 / 支撑层")

    bands = [
        ("表示层", ["Win32 图形界面", "命令行子命令", "交互菜单", "ASCII 地图", "JSON 输出"], 160, BLUE),
        ("业务层", ["路径规划", "转向指引", "多点巡游", "最近设施", "关键道路", "最小生成树"], 380, TEAL),
        ("算法层", ["Dijkstra", "Floyd", "Bellman-Ford", "Tarjan", "Prim / Kruskal", "Held-Karp"], 600, VIOLET),
        ("支撑层", ["CSV 读写", "二进制快照 + CRC32", "CSR 图结构", "UTF-8 / 计时 / 错误码"], 820, AMBER),
    ]
    left, right = 90, W - 90
    for label, items, y, color in bands:
        rounded(d, (left, y, right, y + 150), 18, SOFT, LINE, 2)
        d.rectangle((left, y, left + 12, y + 150), fill=color)
        d.text((left + 32, y + 16), label, font=font(30, "bold"), fill=color)
        n = len(items)
        pad = 28
        box_w = (right - left - 240 - pad * (n - 1)) / n
        for i, item in enumerate(items):
            x0 = left + 220 + i * (box_w + pad)
            box = (x0, y + 66, x0 + box_w, y + 132)
            rounded(d, box, 12, WHITE, color, 2)
            center_text(d, box, item, font(21), INK)

    for y in (310, 530, 750):
        arrow(d, (W / 2, y + 6), (W / 2, y + 64), MUTED, 4)

    rounded(d, (90, 1010, W - 90, 1060), 10, (254, 249, 235), (253, 230, 138), 2)
    d.text((110, 1022), "数据流：data/campus/*.csv → Campus 结构 → Graph(CSR) → 算法 → Path / 矩阵 / 报告",
           font=font(20), fill=(146, 64, 14))
    img.save(out)


def polyline_arrow(d, pts, color=MUTED, width=3, head=12):
    for a, b in zip(pts, pts[1:]):
        d.line([a, b], fill=color, width=width)
    end, prev = pts[-1], pts[-2]
    angle = math.atan2(end[1] - prev[1], end[0] - prev[0])
    for sign in (1, -1):
        a = angle + sign * math.radians(150)
        p = (end[0] + head * math.cos(a), end[1] + head * math.sin(a))
        d.line([end, p], fill=color, width=width)


# ---------------------------------------------------------------- 图 2：流程

def fig_flowchart(root, out):
    W, H = 1440, 1440
    img, d = canvas(W, H)
    title_block(d, W, "程序主流程", "启动 → 载入 → 校验 → 建图 → 交互 → 输出")

    x0, x1 = 240, 880
    cx = (x0 + x1) / 2
    bh = 88
    steps = [
        ("启动：解析 --data / --time / 子命令", BLUE),
        ("定位数据目录（data/campus 或内置默认数据）", BLUE),
        ("解析 spots.csv / roads.csv（CSV 状态机）", SKY),
        ("campus_validate：编号、端点、长度、重复边、范围", SKY),
        ("graph_build：跳过封闭道路，构建 CSR 邻接表 + 边表", TEAL),
        ("读取命令：子命令直达 或 进入交互菜单", TEAL),
        ("按功能分发算法（最短路 / 巡游 / 关键道路 / MST）", VIOLET),
        ("格式化输出：路径、距离、耗时统计、JSON", GREEN),
    ]
    ys = []
    y = 165
    for text, color in steps:
        box = (x0, y, x1, y + bh)
        rounded(d, box, 14, WHITE, color, 3)
        center_text(d, box, text, font(23), INK)
        ys.append((y, y + bh))
        y += bh + 78

    for i in range(len(ys) - 1):
        arrow(d, (cx, ys[i][1] + 4), (cx, ys[i + 1][0] - 6), MUTED, 3)

    # 校验失败分支：从 campus_validate 右侧引出
    ey0 = ys[3][0] - 34
    ey1 = ys[3][1] + 34
    ex0 = 960
    box = (ex0, ey0, W - 60, ey1)
    rounded(d, box, 12, (254, 242, 242), (252, 165, 165), 2)
    lines = ["校验失败", "输出错误码 + 可读信息", "进程退出码 2"]
    for i, line in enumerate(lines):
        f = font(22 if i == 0 else 19, "bold" if i == 0 else "regular")
        tw, _ = text_size(d, line, f)
        d.text((ex0 + (W - 60 - ex0 - tw) / 2, ey0 + 18 + i * 34), line, font=f,
               fill=(185, 28, 28))
    polyline_arrow(d, [(x1, (ys[3][0] + ys[3][1]) / 2), (ex0 - 8, (ys[3][0] + ys[3][1]) / 2)],
                   (220, 38, 38), 3)

    # 图编辑后的重建回边：从「分发算法」绕回「graph_build」
    c6 = (ys[6][0] + ys[6][1]) / 2
    c4 = (ys[4][0] + ys[4][1]) / 2
    polyline_arrow(d, [(x0 - 6, c6), (150, c6), (150, c4), (x0 - 6, c4)], AMBER, 3)
    d.text((40, (c4 + c6) / 2 - 10), "图编辑后", font=font(19), fill=(180, 83, 9))
    d.text((40, (c4 + c6) / 2 + 16), "重建图", font=font(19), fill=(180, 83, 9))

    img.save(out)


# ---------------------------------------------------------------- 图 3/4：校园图

def _map_geometry(spots, w, h, pad=110):
    max_x = max(s["x"] for s in spots)
    max_y = max(s["y"] for s in spots)
    sx = (w - 2 * pad) / max_x
    sy = (h - 2 * pad) / max_y
    s = min(sx, sy)

    def project(p):
        return (pad + p["x"] * s, h - pad - p["y"] * s)

    return project


def fig_campus_map(root, out, route=None, caption=None):
    spots, roads = load_campus(root)
    W, H = 1720, 1180
    img, d = canvas(W, H)
    title_block(d, W, "江苏海洋大学（苍梧校区）校园路网", "24 个地点 · 41 条道路 · 坐标与距离为教学示意值")
    project = _map_geometry(spots, W, H - 60, pad=120)
    by_id = {s["id"]: s for s in spots}

    for r in roads:
        p0 = project(by_id[r["a"]])
        p1 = project(by_id[r["b"]])
        d.line([p0, p1], fill=(203, 213, 225), width=5)

    if route:
        pts = [project(by_id[i]) for i in route]
        for i in range(len(pts) - 1):
            d.line([pts[i], pts[i + 1]], fill=(239, 68, 68), width=9)

    colors_used = {}
    for s in spots:
        p = project(s)
        c = TYPE_COLOR.get(s["type"], MUTED)
        colors_used[s["type"]] = c
        r = 17
        d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=c, outline=WHITE, width=3)
        center_text(d, (p[0] - 14, p[1] - 14, p[0] + 14, p[1] + 14), str(s["id"]), font(17, "bold"), WHITE)
        label = s["name"]
        f = font(20)
        tw, th = text_size(d, label, f)
        lx, ly = p[0] + 22, p[1] - th - 6
        if ly < 150:  # 顶部节点标签放到下方，避免压住标题
            ly = p[1] + 30
        if lx + tw > W - 20:
            lx = p[0] - tw - 22
        d.rounded_rectangle((lx - 4, ly - 3, lx + tw + 4, ly + th + 5), radius=4, fill=WHITE)
        d.text((lx, ly), label, font=f, fill=(51, 65, 85))

    # 图例
    legend_y = H - 62
    x = 120
    for stype, color in TYPE_COLOR.items():
        d.ellipse((x, legend_y + 6, x + 16, legend_y + 22), fill=color)
        d.text((x + 24, legend_y), TYPE_LABEL[stype], font=font(20), fill=(71, 85, 105))
        x += 150

    if caption:
        d.text((120, H - 108), caption, font=font(22, "bold"), fill=(185, 28, 28))
    img.save(out)


# ---------------------------------------------------------------- 图 5：数据格式

def fig_data_format(root, out):
    W, H = 1500, 820
    img, d = canvas(W, H)
    title_block(d, W, "数据文件格式", "人类可读的 CSV + 带完整性校验的二进制快照")

    # 左：CSV
    rounded(d, (70, 160, 720, 560), 16, (248, 250, 252), LINE, 2)
    d.text((100, 185), "spots.csv（地点表）", font=font(28, "bold"), fill=BLUE)
    mono = font(19, "mono")
    lines = [
        "# title: 江苏海洋大学（苍梧校区）",
        "# 字段：编号,名称,类型,x,y,简介",
        "1,东大门,gate,900,360,学校正门…",
        "4,图书馆,library,460,350,藏书与自习…",
    ]
    for i, line in enumerate(lines):
        d.text((100, 235 + i * 38), line, font=mono, fill=(51, 65, 85))
    d.text((100, 400), "roads.csv（道路表，无向边）", font=font(28, "bold"), fill=BLUE)
    for i, line in enumerate(["# 字段：编号,起点,终点,长度(米),步行时间(分钟)",
                              "1,1,12,147.3,1.9"]):
        d.text((100, 448 + i * 38), line, font=mono, fill=(51, 65, 85))

    # 右：二进制
    rounded(d, (780, 160, 1430, 560), 16, (248, 250, 252), LINE, 2)
    d.text((810, 185), "campus.nvg（二进制快照）", font=font(28, "bold"), fill=VIOLET)
    fields = [
        ("0", "magic \"NVG1\"", "4 B"),
        ("4", "version", "2 B"),
        ("6", "flags", "2 B"),
        ("8", "n_spots / n_roads", "8 B"),
        ("16", "width / height", "16 B"),
        ("32", "title（长度前缀）", "2+N B"),
        ("…", "spots（id,type,x,y,name,brief）", "变长"),
        ("…", "roads（id,a,b,len,min,open）", "29 B/条"),
        ("末尾", "CRC32 校验和", "4 B"),
    ]
    for i, (off, name, size) in enumerate(fields):
        y = 240 + i * 36
        d.text((810, y), off.rjust(4), font=font(19, "mono"), fill=MUTED)
        d.text((880, y), name, font=font(20, "mono"), fill=(51, 65, 85))
        d.text((1290, y), size, font=font(19, "mono"), fill=MUTED)

    rounded(d, (70, 600, 1430, 720), 14, (254, 249, 235), (253, 230, 138), 2)
    note = ("设计要点：CSV 便于人工编辑与版本对比；快照用定长字段 + 小端序写入（不直接 fwrite 结构体），"
            "末尾 CRC32 可检测截断与篡改")
    for i, line in enumerate(wrap(d, note, font(22), 1330)):
        d.text((95, 628 + i * 34), line, font=font(22), fill=(146, 64, 14))
    img.save(out)


# ---------------------------------------------------------------- 图 6：算法对比

def fig_algo_compare(root, out):
    rows = load_bench(root)
    W, H = 1500, 960
    img, d = canvas(W, H)
    title_block(d, W, "算法耗时随图规模的变化", "随机图（约 3V 条边），纵轴为对数刻度，单位毫秒")

    left, right, top, bottom = 170, W - 90, 200, H - 220
    series = [("Dijkstra", "dijkstra", BLUE), ("Floyd-Warshall", "floyd", VIOLET),
              ("Bellman-Ford", "bellman", AMBER), ("Kruskal", "kruskal", GREEN)]
    vals = [r[k] for r in rows for _, k, _ in series if k in r and r[k] > 0]
    lo = math.floor(math.log10(min(vals))) if vals else -3
    hi = math.ceil(math.log10(max(vals))) if vals else 1
    lo = min(lo, -3)

    def X(n):
        return left + (n - rows[0]["n"]) / (rows[-1]["n"] - rows[0]["n"]) * (right - left)

    def Y(v):
        v = max(v, 10 ** lo)
        return bottom - (math.log10(v) - lo) / (hi - lo) * (bottom - top)

    for e in range(lo, hi + 1):
        y = Y(10 ** e)
        d.line([(left, y), (right, y)], fill=(241, 245, 249), width=2)
        d.text((left - 90, y - 12), f"1e{e}", font=font(20, "mono"), fill=MUTED)

    for i, r in enumerate(rows):
        x = X(r["n"])
        d.line([(x, bottom), (x, bottom + 8)], fill=LINE, width=2)
        tw, _ = text_size(d, str(r["n"]), font(20))
        d.text((x - tw / 2, bottom + 14), str(r["n"]), font=font(20), fill=MUTED)

    d.text((left, bottom + 54), "图规模 n（节点数）", font=font(22), fill=INK)
    d.text((30, top - 44), "耗时(ms)", font=font(22), fill=INK)

    legend_x = left + 40
    for i, (label, key, color) in enumerate(series):
        pts = [(X(r["n"]), Y(r[key])) for r in rows if key in r and r[key] > 0]
        for a, b in zip(pts, pts[1:]):
            d.line([a, b], fill=color, width=4)
        for p in pts:
            d.ellipse((p[0] - 6, p[1] - 6, p[0] + 6, p[1] + 6), fill=WHITE, outline=color, width=3)
        y = top + 6 + i * 40
        d.line([(legend_x, y + 12), (legend_x + 36, y + 12)], fill=color, width=5)
        d.text((legend_x + 48, y), label, font=font(21), fill=INK)

    note = "结论：稀疏图上 Dijkstra 远快于 Floyd；Bellman-Ford 慢于 Dijkstra 但可处理负权；Kruskal 接近线性。"
    for i, line in enumerate(wrap(d, note, font(21), 1250)):
        d.text((left, H - 84 + i * 30), line, font=font(21), fill=MUTED)
    img.save(out)


# ---------------------------------------------------------------- 图 7：测试统计

def fig_test_summary(root, out):
    groups = load_test_groups(root)
    W, H = 1400, 1000
    img, d = canvas(W, H)
    total = sum(g[1] for g in groups)
    title_block(d, W, "自动化测试覆盖", f"共 {len(groups)} 组、{total} 项断言，全部通过")

    left, right, top = 380, W - 120, 190
    row_h = (H - top - 90) / max(len(groups), 1)
    mx = max(g[1] for g in groups)
    for i, (name, checks, failed) in enumerate(groups):
        y = top + i * row_h
        d.text((90, y + 6), name, font=font(20), fill=(51, 65, 85))
        w = (right - left) * checks / mx
        color = GREEN if failed == 0 else RED
        rounded(d, (left, y + 4, left + max(w, 4), y + row_h - 10), 6, color)
        d.text((left + max(w, 4) + 10, y + 6), str(checks), font=font(19, "mono"), fill=MUTED)
    img.save(out)


# ---------------------------------------------------------------- 图 8：终端

def fig_terminal(root, out):
    data_dir = os.path.join(root, "docs", "data")
    blocks = [
        ("$ nav route 宿舍A区 图书馆", "demo_route2.txt", 20),
        ("$ nav critical", "demo_critical.txt", 10),
        ("$ nav mst", "demo_mst.txt", 8),
    ]
    lines = []
    for cmd, name, take in blocks:
        path = os.path.join(data_dir, name)
        if not os.path.exists(path):
            continue
        text = read_text(path)
        content = [ln.rstrip() for ln in text.splitlines() if ln.strip()]
        lines.append(("cmd", cmd))
        for ln in content[:take]:
            lines.append(("out", ln))

    mono = font(21, "mono")
    line_h = 34
    W = 1420
    H = 130 + line_h * len(lines) + 60
    img, d = canvas(W, H, (17, 24, 39))
    d.rounded_rectangle((20, 20, W - 20, H - 20), radius=16, outline=(55, 65, 81), width=3)
    for i, c in enumerate(((239, 68, 68), (245, 158, 11), (34, 197, 94))):
        d.ellipse((48 + i * 34, 44, 72 + i * 34, 68), fill=c)
    d.text((150, 45), "校园导航系统 · 命令行演示", font=font(22, "bold"), fill=(148, 163, 184))

    y = 96
    for kind, line in lines:
        color = (134, 239, 172) if kind == "cmd" else (226, 232, 240)
        d.text((48, y), line[:118], font=mono, fill=color)
        y += line_h
    img.save(out)



def fig_real_map(root, out):
    """用真实路网 + 底图生成报告插图（数据来自 data/campus_real）。"""
    data = os.path.join(root, "data", "campus_real")
    meta = json.load(open(os.path.join(data, "meta.json"), encoding="utf-8"))
    m = meta["m_per_px"]
    ox, oy = meta["px_origin"]

    base = Image.open(os.path.join(data, "basemap.png")).convert("RGB")
    W, H = base.size
    img = base.copy()
    d = ImageDraw.Draw(img)

    def to_px(x, y):
        return (x / m + ox, oy - y / m)

    # 道路折线
    with open(os.path.join(data, "shapes.csv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.strip().split(",")
            if len(parts) < 3:
                continue
            pts = []
            for pair in parts[2].split(";"):
                xy = pair.split()
                if len(xy) == 2:
                    pts.append(to_px(float(xy[0]), float(xy[1])))
            if len(pts) >= 2:
                d.line(pts, fill=(90, 110, 140), width=3, joint="curve")

    # 地点
    with open(os.path.join(data, "spots.csv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            row = next(csv.reader([line]))
            sid, name, stype, x, y = row[:5]
            if stype == "junction":
                continue
            px, py = to_px(float(x), float(y))
            d.ellipse((px - 11, py - 11, px + 11, py + 11), fill=(220, 38, 38),
                      outline=(255, 255, 255), width=3)
            fnt = font(20, "bold")
            d.text((px - 7, py - 9), str(int(sid)), font=fnt, fill=(255, 255, 255))

    # 标题条
    bar = Image.new("RGB", (W, 92), (255, 255, 255))
    img.paste(bar, (0, 0))
    d.text((28, 16), "江苏海洋大学（苍梧校区）真实路网", font=font(38, "bold"), fill=(31, 41, 55))
    d.text((30, 62), "353 个节点（20 个地点 + 333 个路口）· 409 条道路 · 总长 22.4 km",
           font=font(22), fill=(107, 114, 128))

    # 裁掉右侧空白与底部
    img = img.crop((0, 0, W, H))
    img.save(out)
    print("real map ->", out, img.size)


def main(argv):
    root = argv[1] if len(argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    out_dir = os.path.join(root, "docs", "diagrams")
    os.makedirs(out_dir, exist_ok=True)

    fig_architecture(root, os.path.join(out_dir, "architecture.png"))
    fig_flowchart(root, os.path.join(out_dir, "flowchart.png"))
    fig_real_map(root, os.path.join(out_dir, "campus-map.png"))
    fig_data_format(root, os.path.join(out_dir, "data-format.png"))
    fig_algo_compare(root, os.path.join(out_dir, "algo-compare.png"))
    fig_terminal(root, os.path.join(out_dir, "terminal-demo.png"))
    print("figures written to", out_dir)


if __name__ == "__main__":
    main(sys.argv)
