#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在真实底图上渲染一条最短路径（报告/答辩用图）。

用法：
  python render_route.py --from 1 --to 5 --out docs/diagrams/route_gate_library.png \
      --title "1号门 → 图书馆"

数据来自 data/campus_real/{spots,roads}.csv；路径由内置 Dijkstra 计算，
并会调用 build/nav.exe 复核（若可执行），保证图与程序输出一致。
"""
import argparse
import csv
import heapq
import json
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, "data", "campus_real")
GRAPH = os.path.join(DATA, "campus_graph.json")
BASE_CANDIDATES = [
    os.path.join(ROOT, "data", "campus_real", "basemap.png"),
    os.path.join(ROOT, "build", "road_data", "work_base.png"),
]
BASE = next((p for p in BASE_CANDIDATES if os.path.exists(p)), BASE_CANDIDATES[-1])
NAV = os.path.join(ROOT, "build", "nav.exe")
M_PER_PX = 0.492

FONT = r"C:\Windows\Fonts\msyh.ttc"
FONT_B = r"C:\Windows\Fonts\msyhbd.ttc"


def load_graph():
    return json.load(open(GRAPH, encoding="utf-8"))


def load_roads(graph):
    """邻接表直接用 graph.json 的边（节点 id 与图形一致，如 N123 / P01）"""
    adj = {}
    for e in graph["edges"]:
        adj.setdefault(e["a"], []).append((e["b"], e["len_m"]))
        adj.setdefault(e["b"], []).append((e["a"], e["len_m"]))
    return adj


def dijkstra(adj, src, dst):
    dist = {src: 0.0}
    prev = {}
    pq = [(0.0, src)]
    seen = set()
    while pq:
        d, u = heapq.heappop(pq)
        if u in seen:
            continue
        seen.add(u)
        if u == dst:
            break
        for v, w in adj.get(u, []):
            nd = d + w
            if nd < dist.get(v, 1e18) - 1e-9:
                dist[v] = nd
                prev[v] = u
                heapq.heappush(pq, (nd, v))
    if dst not in dist:
        return None, None
    path = [dst]
    while path[-1] != src:
        path.append(prev[path[-1]])
    path.reverse()
    return path, dist[dst]


def path_polyline(graph, path):
    """把节点序列还原成像素折线（用边上的 shape）"""
    by_pair = {}
    for e in graph["edges"]:
        by_pair[(e["a"], e["b"])] = e["shape"]
        by_pair[(e["b"], e["a"])] = list(reversed(e["shape"]))
    pts = []
    for a, b in zip(path, path[1:]):
        shape = by_pair.get((a, b))
        if not shape:
            na = next(n for n in graph["nodes"] if n["id"] == a)
            nb = next(n for n in graph["nodes"] if n["id"] == b)
            shape = [[na["x"], na["y"]], [nb["x"], nb["y"]]]
        if pts and shape:
            shape = shape[1:]
        pts.extend([tuple(p) for p in shape])
    return pts


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src", type=int, required=True)
    ap.add_argument("--to", dest="dst", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--title", default="")
    args = ap.parse_args(argv[1:])

    graph = load_graph()
    adj = load_roads(graph)
    src_id = f"P{args.src:02d}"
    dst_id = f"P{args.dst:02d}"
    path, total = dijkstra(adj, src_id, dst_id)
    if path is None:
        print("no path")
        return 1

    # 用程序复核
    if os.path.exists(NAV):
        try:
            r = subprocess.run([NAV, "--data", DATA, "route", str(args.src), str(args.dst), "--json"],
                               capture_output=True, text=True, encoding="utf-8",
                               errors="replace", timeout=30)
            if r.returncode == 0:
                out = (r.stdout or "").strip().splitlines()
                j = json.loads(out[-1]) if out else {}
                if abs(float(j.get("weight", total)) - total) > 0.5:
                    print(f"WARNING: figure path {total:.1f} m != program {j['weight']} m")
                else:
                    print(f"program check ok: {j['weight']:.1f} m, {len(j['nodes'])} nodes")
        except Exception as exc:  # noqa: BLE001
            print("program check skipped:", exc)

    nodes = {n["id"]: n for n in graph["nodes"]}
    pts = path_polyline(graph, path)

    # 裁剪范围：路径 + 起终点 + 余量
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    pad = 160
    x0 = max(0, int(min(xs) - pad))
    y0 = max(0, int(min(ys) - pad))
    x1 = int(max(xs) + pad)
    y1 = int(max(ys) + pad)

    im = Image.open(BASE).convert("RGB")
    x1 = min(x1, im.width)
    y1 = min(y1, im.height)
    crop = im.crop((x0, y0, x1, y1)).convert("RGB")

    # 稍微提亮道路，让红线更突出
    dr = ImageDraw.Draw(crop, "RGBA")

    def tp(p):
        return (p[0] - x0, p[1] - y0)

    screen = [tp(p) for p in pts]
    dr.line(screen, fill=(255, 255, 255, 235), width=13, joint="curve")
    dr.line(screen, fill=(214, 40, 40, 255), width=7, joint="curve")

    f_pt = ImageFont.truetype(FONT_B, 30)
    f_t = ImageFont.truetype(FONT_B, 46)
    f_s = ImageFont.truetype(FONT, 24)

    src_p = tp((nodes[path[0]]["x"], nodes[path[0]]["y"]))
    dst_p = tp((nodes[path[-1]]["x"], nodes[path[-1]]["y"]))
    for p, color, label in ((src_p, (22, 163, 74), "起"), (dst_p, (220, 38, 38), "终")):
        dr.ellipse((p[0] - 16, p[1] - 16, p[0] + 16, p[1] + 16), fill=color,
                   outline=(255, 255, 255), width=4)
        dr.text((p[0] - 15, p[1] - 18), label, font=ImageFont.truetype(FONT_B, 22), fill="white")

    # 标题条
    title = args.title or f"{nodes[path[0]].get('name','')} → {nodes[path[-1]].get('name','')}"
    bar_h = 96
    dr.rectangle((0, 0, crop.width, bar_h), fill=(255, 255, 255, 235))
    dr.text((24, 16), title, font=f_t, fill=(31, 41, 55))
    info = f"最短距离 {total:.1f} 米   步行约 {total / 78.0:.1f} 分钟   经过 {len(path)} 个节点"
    dr.text((26, 64), info, font=f_s, fill=(90, 100, 115))

    # 比例尺（0.492 m/px）
    bar_m = 100
    bar_px = bar_m / M_PER_PX
    bx, by = 34, crop.height - 58
    dr.rectangle((bx, by, bx + bar_px, by + 8), fill=(40, 40, 40, 255))
    dr.rectangle((bx + bar_px, by, bx + 2 * bar_px, by + 8), fill=(255, 255, 255, 255),
                 outline=(40, 40, 40, 255))
    dr.text((bx, by - 30), "0", font=f_s, fill=(40, 40, 40))
    dr.text((bx + bar_px - 10, by - 30), "100", font=f_s, fill=(40, 40, 40))
    dr.text((bx + 2 * bar_px - 8, by - 30), "200 m", font=f_s, fill=(40, 40, 40))

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    crop.save(args.out)
    print(f"{title}: {total:.1f} m, crop {crop.size} -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
