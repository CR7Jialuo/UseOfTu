# -*- coding: utf-8 -*-
"""把 campus_graph.json 导出为程序可读的 spots.csv / roads.csv。

编号规则：POI 用地图像上的编号 1..20（与图例一致），路口/接驳点从 21 起顺排，
类型记作 junction（后续在程序里隐藏显示）。
"""
import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "data", "campus_real")

CLS_MIN_PER_M = {"campus": 1.2 / 60.0, "arterial": 1.4 / 60.0, "link": 1.0 / 60.0}


def main():
    g = json.load(open(os.path.join(OUT, "campus_graph.json"), encoding="utf-8"))
    nodes = g["nodes"]
    edges = g["edges"]

    poi_by_id = {p["id"]: p for p in g["poi"]}
    node_meta = {n["id"]: n for n in nodes}

    # 编号：POI 用原编号；其它从 21 起
    id_map = {}
    used = set()
    for pid in sorted(poi_by_id):
        id_map[f"P{pid:02d}"] = pid
        used.add(pid)
    nxt = 21
    for n in nodes:
        if n["id"] in id_map:
            continue
        while nxt in used:
            nxt += 1
        id_map[n["id"]] = nxt
        used.add(nxt)
        nxt += 1

    # 坐标：转成米制（原点取西南角），y 轴翻转
    xs = [n["x"] for n in nodes]
    ys = [n["y"] for n in nodes]
    minx, maxy = min(xs), max(ys)
    m = g["scale_m_per_px"]

    def to_m(n):
        return ((n["x"] - minx) * m, (maxy - n["y"]) * m)

    # 检查边端点、去重
    seen = set()
    rows = []
    skipped = 0
    for e in edges:
        a, b = id_map[e["a"]], id_map[e["b"]]
        if a == b:
            skipped += 1
            continue
        key = (min(a, b), max(a, b))
        if key in seen:
            skipped += 1
            continue
        seen.add(key)
        length = e["len_m"]
        if length <= 0:
            skipped += 1
            continue
        rows.append((a, b, length, e["cls"], e["name"]))

    # 节点编号映射（供 GUI/渲染脚本使用）
    with open(os.path.join(OUT, "node_ids.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# graph_id,numeric_id,kind\n")
        for n in nodes:
            f.write(f"{n['id']},{id_map[n['id']]},{n['kind']}\n")

    # 写 spots
    with open(os.path.join(OUT, "spots.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# title: 江苏海洋大学（苍梧校区）\n")
        f.write("# 字段：编号,名称,类型,横坐标(米),纵坐标(米),简介\n")
        f.write("# 坐标原点在校园西南角；1-20 为地图编号地点，21 起为路网路口\n")
        ordered = sorted(nodes, key=lambda n: id_map[n["id"]])
        for n in ordered:
            sid = id_map[n["id"]]
            x, y = to_m(n)
            if n["kind"] == "poi":
                name = n["name"]
                stype = n["type"]
                brief = f"地图编号 {sid:02d}"
            else:
                name = "路口" + str(sid)
                stype = "junction"
                brief = "路网节点"
            f.write(f"{sid},{name},{stype},{x:.1f},{y:.1f},{brief}\n")

    # 写 roads
    with open(os.path.join(OUT, "roads.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# 字段：编号,起点编号,终点编号,长度(米),步行时间(分钟),路名\n")
        for i, (a, b, length, cls, name) in enumerate(sorted(rows), start=1):
            minutes = length * CLS_MIN_PER_M.get(cls, 1.2 / 60.0)
            f.write(f"{i},{a},{b},{length:.1f},{max(minutes, 0.1):.1f},{name}\n")

    total = sum(r[2] for r in rows)
    print(f"spots={len(nodes)} roads={len(rows)} (skipped {skipped}) total={total:.0f} m")
    print("POI ids:", sorted(poi_by_id))
    # 连通性自检
    adj = {}
    for a, b, *_ in rows:
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    start = next(iter(adj))
    seen_n = {start}
    stack = [start]
    while stack:
        cur = stack.pop()
        for nxt2 in adj.get(cur, []):
            if nxt2 not in seen_n:
                seen_n.add(nxt2)
                stack.append(nxt2)
    print(f"connected: {len(seen_n)} / {len(nodes)} nodes")
    if len(seen_n) != len(nodes):
        missing = [n for n in id_map.values() if n not in seen_n]
        print("  disconnected sample:", missing[:10])


if __name__ == "__main__":
    main()
