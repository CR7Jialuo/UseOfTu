# -*- coding: utf-8 -*-
"""分析并化简外部路网：
  1) 清理自环/重复边/零长边；
  2) 取最大连通片；
  3) 度数为 2 的节点合并进折线（保留 ≥3 度路口与 1 度尽端）；
  4) 输出化简后的路口图（节点/边/累计长度）与统计。
"""
import csv
import json
import math
import os
from collections import defaultdict, deque

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "build", "road_data")
OUT = os.path.join(ROOT, "data", "campus_real")
os.makedirs(OUT, exist_ok=True)


def load():
    nodes = {}
    with open(os.path.join(SRC, "路网拓扑_nodes.csv"), encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            nodes[r["node_id"]] = {
                "id": r["node_id"],
                "x": float(r["x_gcj02"]),
                "y": float(r["y_gcj02"]),
                "lon": float(r["lon_wgs84"]),
                "lat": float(r["lat_wgs84"]),
                "degree": int(r["degree"]),
            }
    edges = []
    with open(os.path.join(SRC, "路网拓扑_edges.csv"), encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            edges.append({
                "id": r["edge_id"],
                "a": r["node_a"],
                "b": r["node_b"],
                "len": float(r["length_m"]),
                "cls": r["class"],
                "name": r["name"],
            })
    return nodes, edges


def main():
    nodes, edges = load()
    print(f"raw: {len(nodes)} nodes, {len(edges)} edges")

    # 1) 清理
    seen = set()
    clean = []
    dropped = {"self": 0, "dup": 0, "zero": 0, "missing": 0}
    for e in edges:
        a, b = e["a"], e["b"]
        if a not in nodes or b not in nodes:
            dropped["missing"] += 1
            continue
        if a == b:
            dropped["self"] += 1
            continue
        key = tuple(sorted((a, b)))
        if key in seen:
            dropped["dup"] += 1
            continue
        if e["len"] <= 0.05:
            dropped["zero"] += 1
            continue
        seen.add(key)
        clean.append(e)
    print("dropped:", dropped, "-> clean edges:", len(clean))

    adj = defaultdict(list)
    for e in clean:
        adj[e["a"]].append(e)
        adj[e["b"]].append(e)

    # 2) 最大连通片
    seen_n = set()
    comps = []
    for n in nodes:
        if n in seen_n:
            continue
        q = deque([n])
        seen_n.add(n)
        comp = []
        while q:
            cur = q.popleft()
            comp.append(cur)
            for e in adj[cur]:
                nxt = e["b"] if e["a"] == cur else e["a"]
                if nxt not in seen_n:
                    seen_n.add(nxt)
                    q.append(nxt)
        comps.append(comp)
    comps.sort(key=len, reverse=True)
    main_comp = set(comps[0])
    print(f"components: {len(comps)}, largest={len(comps[0])}, "
          f"others={[len(c) for c in comps[1:6]]}")

    keep_edges = [e for e in clean if e["a"] in main_comp and e["b"] in main_comp]
    deg = defaultdict(int)
    for e in keep_edges:
        deg[e["a"]] += 1
        deg[e["b"]] += 1

    # 3) 化简：保留 deg != 2 的节点
    keep_nodes = {n for n in main_comp if deg[n] != 2}
    print(f"nodes after degree-2 removal: {len(keep_nodes)} "
          f"(junctions deg>=3: {sum(1 for n in main_comp if deg[n] >= 3)}, "
          f"dead ends deg=1: {sum(1 for n in main_comp if deg[n] == 1)})")

    # 沿折线从每个保留节点走到下一个保留节点，累计长度
    simplified = []
    visited_dir = set()
    for start in keep_nodes:
        for e0 in adj[start]:
            if e0["id"] in visited_dir:
                continue
            path_ids = [start]
            path_pts = [(nodes[start]["x"], nodes[start]["y"])]
            cur = start
            cur_edge = e0
            total = 0.0
            names = []
            classes = []
            ok = True
            guard = 0
            while True:
                guard += 1
                if guard > 5000:
                    ok = False
                    break
                e = cur_edge
                nxt = e["b"] if e["a"] == cur else e["a"]
                visited_dir.add(e["id"])
                total += e["len"]
                names.append(e["name"])
                classes.append(e["cls"])
                path_pts.append((nodes[nxt]["x"], nodes[nxt]["y"]))
                path_ids.append(nxt)
                if nxt in keep_nodes:
                    break
                # 继续走另一条边
                nxt_edges = [x for x in adj[nxt] if x["id"] != e["id"]]
                if len(nxt_edges) != 1:
                    ok = False
                    break
                cur = nxt
                cur_edge = nxt_edges[0]
            if ok and len(path_ids) >= 2:
                simplified.append({
                    "a": path_ids[0],
                    "b": path_ids[-1],
                    "points": path_ids,
                    "pts_xy": path_pts,
                    "len": round(total, 1),
                    "cls": max(set(classes), key=classes.count),
                    "name": max(set(names), key=names.count),
                })

    # 去重（同一对端点可能正反向各记录一次）
    dedup = {}
    for s in simplified:
        key = tuple(sorted((s["a"], s["b"])))
        if key not in dedup or s["len"] < dedup[key]["len"]:
            dedup[key] = s
    final = list(dedup.values())
    total_len = sum(s["len"] for s in final)
    print(f"simplified: {len(final)} edges, total {total_len:.0f} m")

    # 保留节点（化简后出现过的）
    used_nodes = sorted({s["a"] for s in final} | {s["b"] for s in final})
    print(f"kept nodes: {len(used_nodes)}")

    out = {
        "scale_m_per_px": 0.492,
        "image_size": [2572, 2582],
        "nodes": [{"id": n, "x": nodes[n]["x"], "y": nodes[n]["y"],
                   "lon": nodes[n]["lon"], "lat": nodes[n]["lat"], "degree": deg[n]}
                  for n in used_nodes],
        "edges": final,
    }
    json.dump(out, open(os.path.join(OUT, "network_simplified.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    print("->", os.path.join(OUT, "network_simplified.json"))


if __name__ == "__main__":
    main()
