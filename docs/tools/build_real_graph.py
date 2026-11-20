# -*- coding: utf-8 -*-
"""由底图坐标生成真实校园图：
   - 道路用折线定义（像素坐标，人工读图）
   - POI 投影到最近折线段，生成连接支路
   - 输出 spots.csv / roads.csv（米制）与叠加校验图
"""
import csv
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BASE = os.path.join(ROOT, "build", "template", "campus_map.png")
POI_JSON = os.path.join(ROOT, "build", "real_map", "poi.json")
OUT_DIR = os.path.join(ROOT, "data", "campus_real")

M_PER_PX = 200.0 / 272.0

# 道路折线（像素坐标）。type: main 主干道 / branch 支路 / path 步道
ROADS = [
    # 东西向
    ("苍梧路", "main", [(300, 1595), (700, 1570), (1150, 1545), (1500, 1560)]),
    ("香海路", "main", [(700, 487), (1000, 480), (1330, 473)]),
    ("月季路", "branch", [(700, 650), (1000, 642), (1330, 636)]),
    ("樱花路", "branch", [(700, 1000), (1010, 992), (1300, 986)]),
    ("文华路", "branch", [(560, 1190), (950, 1182), (1330, 1173)]),
    ("风华路", "branch", [(540, 1300), (900, 1292), (1250, 1278)]),
    # 南北向
    ("科苑中路", "main", [(330, 420), (305, 1000), (288, 1560)]),
    ("学院路", "main", [(1330, 450), (1352, 1000), (1372, 1550)]),
    ("学海路", "branch", [(700, 487), (700, 1000), (702, 1300), (704, 1560)]),
    ("文苑路", "branch", [(872, 475), (878, 1000), (884, 1290), (890, 1530)]),
    ("春晖路", "branch", [(1240, 475), (1248, 1000), (1256, 1290), (1262, 1450)]),
]


SRC_JSON = os.path.join(ROOT, "data", "campus_real", "roads_source.json")


def load_roads():
    """优先读取外部导入的折线（KML/GPX/JSON），否则用脚本内的初版折线。"""
    if os.path.exists(SRC_JSON):
        data = json.load(open(SRC_JSON, encoding="utf-8"))
        return [(r["name"], r.get("type", "branch"),
                 [tuple(pt) for pt in r["points_px"]]) for r in data["roads"]]
    return ROADS


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def project_on_segment(p, a, b):
    """返回 (距离, 垂足点, 参数 t)"""
    ax, ay = a
    bx, by = b
    px, py = p
    dx, dy = bx - ax, by - ay
    seg2 = dx * dx + dy * dy
    if seg2 == 0:
        return dist(p, a), a, 0.0
    t = ((px - ax) * dx + (py - ay) * dy) / seg2
    t = max(0.0, min(1.0, t))
    foot = (ax + t * dx, ay + t * dy)
    return dist(p, foot), foot, t


def main():
    roads = load_roads()
    poi = json.load(open(POI_JSON, encoding="utf-8"))
    pois = poi["pois"]
    minx, maxy = poi["minx"], poi["maxy"]

    def to_m(px, py):
        return ((px - minx) * M_PER_PX, (maxy - py) * M_PER_PX)

    # ---- 节点：折线顶点 + 投影点 ----
    nodes = []          # (px, py, kind, label)
    node_index = {}

    def add_node(px, py, kind, label=""):
        key = (round(px, 1), round(py, 1))
        if key in node_index:
            return node_index[key]
        nodes.append({"px": px, "py": py, "kind": kind, "label": label})
        node_index[key] = len(nodes) - 1
        return node_index[key]

    edges = []          # (i, j, road_name, type)

    for name, rtype, pts in roads:
        ids = [add_node(x, y, "junction", name) for x, y in pts]
        for a, b in zip(ids, ids[1:]):
            edges.append((a, b, name, rtype))

    # 为每条道路记录“插入点”，按 t 排序后串起分段
    road_inserts = {r[0]: [] for r in roads}
    poi_attach = {}

    for pid_str, p in pois.items():
        pid = int(pid_str)
        best = None
        for name, rtype, pts in roads:
            for k in range(len(pts) - 1):
                d, foot, t = project_on_segment((p["px"], p["py"]), pts[k], pts[k + 1])
                if best is None or d < best[0]:
                    best = (d, foot, name, rtype, k, t)
        d, foot, name, rtype, seg_k, t = best
        nid = add_node(foot[0], foot[1], "attach", name)
        road_inserts[name].append((seg_k, t, nid, pid))
        poi_attach[pid] = nid

    # 重建道路：把插入点按 (段号, t) 排序，插入到顶点序列中
    edges = []
    for name, rtype, pts in roads:
        seq = [(k, 0.0, add_node(x, y, "junction", name), None) for k, (x, y) in enumerate(pts)]
        seq += road_inserts[name]
        # 排序：先按段号，再按 t（顶点 t=0）
        seq.sort(key=lambda s: (s[0], s[1]))
        # 去重相邻相同节点
        chain = [seq[0][2]]
        for item in seq[1:]:
            if item[2] != chain[-1]:
                chain.append(item[2])
        for a, b in zip(chain, chain[1:]):
            edges.append((a, b, name, rtype))

    # POI -> 投影点 的支路
    for pid_str, p in pois.items():
        pid = int(pid_str)
        poi_node = add_node(p["px"], p["py"], "poi", p["name"])
        edges.append((poi_node, poi_attach[pid], "连接步道", "path"))
        nodes[poi_node]["poi_id"] = pid

    # ---- 输出米制 CSV ----
    os.makedirs(OUT_DIR, exist_ok=True)
    spots = []
    for pid_str, p in sorted(pois.items(), key=lambda kv: int(kv[0])):
        pid = int(pid_str)
        x, y = to_m(p["px"], p["py"])
        spots.append({
            "id": pid, "name": p["name"], "type": p["type"],
            "x": round(x, 1), "y": round(y, 1),
            "brief": f"地图编号 {pid:02d}",
        })

    # 交叉口/投影点也作为地点？不——它们是纯路网节点，用另一种文件表达。
    # 为兼容现有程序，这里把路网节点写成 roads.csv 的端点编号（>100）
    road_rows = []
    rid = 0
    for i, j, name, rtype in edges:
        a, b = nodes[i], nodes[j]
        if a["kind"] == "poi" and b["kind"] == "attach":
            continue  # 支路单独处理
    # 生成：所有边 -> (起点编号, 终点编号, 长度, 时间)
    # 节点编号规则：POI 用 1..20；路网节点从 101 开始
    net_ids = {}
    next_id = 101
    for idx, n in enumerate(nodes):
        if n["kind"] == "poi":
            net_ids[idx] = n["poi_id"]
        else:
            net_ids[idx] = next_id
            next_id += 1

    speed = {"main": 1.4, "branch": 1.2, "path": 1.0}
    rows = []
    for i, j, name, rtype in edges:
        a, b = nodes[i], nodes[j]
        length = dist((a["px"], a["py"]), (b["px"], b["py"])) * M_PER_PX
        if length < 1.0:
            continue
        rid += 1
        rows.append({
            "id": rid, "a": net_ids[i], "b": net_ids[j],
            "name": name, "type": rtype,
            "len": round(length, 1),
            "min": round(length / speed.get(rtype, 1.2) / 60.0, 2),
        })

    # 校验图
    im = Image.open(BASE).convert("RGB")
    d = ImageDraw.Draw(im)
    font = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 13)
    for i, j, name, rtype in edges:
        a, b = nodes[i], nodes[j]
        color = {"main": (200, 30, 30), "branch": (30, 120, 220), "path": (30, 160, 60)}[rtype]
        d.line([(a["px"], a["py"]), (b["px"], b["py"])], fill=color, width=3)
    for n in nodes:
        if n["kind"] == "poi":
            d.ellipse((n["px"] - 5, n["py"] - 5, n["px"] + 5, n["py"] + 5), fill=(255, 0, 255))
        else:
            d.ellipse((n["px"] - 4, n["py"] - 4, n["px"] + 4, n["py"] + 4),
                      fill=(255, 200, 0), outline=(120, 90, 0))
    map_w = int(im.width * 0.735)
    im.crop((0, 0, map_w, im.height)).save(os.path.join(OUT_DIR, "graph_check.png"))
    im.crop((0, 0, map_w, im.height)).resize((map_w // 2, im.height // 2)).save(
        os.path.join(ROOT, "build", "real_map", "graph_check_small.png"))

    # 写 JSON（节点/边，含类型与路名）—— 供下一步生成 CSV 与渲染使用
    graph = {
        "m_per_px": M_PER_PX,
        "nodes": [{"id": i, "px": n["px"], "py": n["py"], "kind": n["kind"],
                   "label": n.get("label", ""), "poi_id": n.get("poi_id")}
                  for i, n in enumerate(nodes)],
        "edges": [{"a": a, "b": b, "name": name, "type": rtype,
                   "len_m": round(dist((nodes[a]["px"], nodes[a]["py"]),
                                       (nodes[b]["px"], nodes[b]["py"])) * M_PER_PX, 1),
                   "min": round(dist((nodes[a]["px"], nodes[a]["py"]),
                                     (nodes[b]["px"], nodes[b]["py"])) * M_PER_PX
                                / speed.get(rtype, 1.2) / 60.0, 2)}
                  for a, b, name, rtype in edges],
        "spots": spots,
    }
    json.dump(graph, open(os.path.join(OUT_DIR, "graph.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)

    total_m = sum(e["len_m"] for e in graph["edges"])
    print(f"nodes={len(nodes)} edges={len(graph['edges'])} spots={len(spots)} "
          f"total_length={total_m:.0f} m")
    print("graph json ->", os.path.join(OUT_DIR, "graph.json"))
    print("check image ->", os.path.join(OUT_DIR, "graph_check.png"))


if __name__ == "__main__":
    main()
