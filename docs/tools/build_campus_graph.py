# -*- coding: utf-8 -*-
"""生成最终校园图：路网（你的坐标系）+ 20 个 POI 投影接入。

要点：
  - POI 只通过一条"支路"接到路网，不插进主链（避免 路口→POI→路口）；
  - 若投影点落在边的端点附近，直接接到该端点；
  - 输出 campus_graph.json（含折线形状，供渲染）与覆盖校验图。
"""
import json
import math
import os
from collections import Counter

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NET = os.path.join(ROOT, "data", "campus_real", "network_simplified.json")
POI = os.path.join(ROOT, "build", "real_map", "poi.json")
BASE = os.path.join(ROOT, "build", "road_data", "路网校对图.png")
OUT = os.path.join(ROOT, "data", "campus_real")

S = 1.5086                 # 我的像素 -> 路网像素
TX, TY = -14.4, 16.5
M_PER_PX = 0.492

NEAR_ENDPOINT_M = 3.0      # 投影点距端点小于该值(米)时直接接端点


def to_theirs(px, py):
    return (S * px + TX, S * py + TY)


def plen(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))


def project(p, pts):
    best = (1e18, 0.0, None)
    acc = 0.0
    for a, b in zip(pts, pts[1:]):
        ax, ay = a
        bx, by = b
        dx, dy = bx - ax, by - ay
        seg2 = dx * dx + dy * dy
        if seg2 <= 1e-9:
            continue
        t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / seg2))
        foot = (ax + t * dx, ay + t * dy)
        d = math.hypot(p[0] - foot[0], p[1] - foot[1])
        if d < best[0]:
            best = (d, acc + t * math.sqrt(seg2), foot)
        acc += math.sqrt(seg2)
    return best


def slice_poly(pts, s0, s1):
    out = []
    acc = 0.0
    for a, b in zip(pts, pts[1:]):
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        end = acc + seg
        if seg <= 0 or end < s0 or acc > s1:
            acc = end
            continue
        t0 = max(0.0, (s0 - acc) / seg)
        t1 = min(1.0, (s1 - acc) / seg)
        if not out:
            out.append((a[0] + t0 * (b[0] - a[0]), a[1] + t0 * (b[1] - a[1])))
        out.append((a[0] + t1 * (b[0] - a[0]), a[1] + t1 * (b[1] - a[1])))
        acc = end
    return out


def main():
    net = json.load(open(NET, encoding="utf-8"))
    poi = json.load(open(POI, encoding="utf-8"))["pois"]

    nodes = {}
    for n in net["nodes"]:
        nodes[n["id"]] = {"x": n["x"], "y": n["y"], "kind": "junction"}

    poi_node = {}
    for pid, p in poi.items():
        nid = f"P{int(pid):02d}"
        x, y = to_theirs(p["px"], p["py"])
        nodes[nid] = {"x": x, "y": y, "kind": "poi", "name": p["name"], "type": p["type"]}
        poi_node[int(pid)] = nid

    # 每个 POI 找最近边
    best_of_poi = {}
    for pid, nid in poi_node.items():
        p = nodes[nid]
        best = (1e18, None, None, None)
        for ei, e in enumerate(net["edges"]):
            d, s, foot = project((p["x"], p["y"]), [tuple(q) for q in e["pts_xy"]])
            if d < best[0]:
                best = (d, ei, s, foot)
        best_of_poi[pid] = best

    cuts = {}
    spurs = []
    for pid, (d, ei, s, foot) in best_of_poi.items():
        nid = poi_node[pid]
        pts = [tuple(q) for q in net["edges"][ei]["pts_xy"]]
        total = plen(pts)
        if s <= NEAR_ENDPOINT_M / M_PER_PX:
            spurs.append((nid, net["edges"][ei]["a"]))
        elif s >= total - NEAR_ENDPOINT_M / M_PER_PX:
            spurs.append((nid, net["edges"][ei]["b"]))
        else:
            aid = f"A{pid:02d}"
            nodes[aid] = {"x": foot[0], "y": foot[1], "kind": "attach"}
            cuts.setdefault(ei, []).append((s, aid))
            spurs.append((nid, aid))

    edges = []
    for ei, e in enumerate(net["edges"]):
        pts = [tuple(q) for q in e["pts_xy"]]
        total = plen(pts)
        chain = [(0.0, e["a"])] + sorted(cuts.get(ei, []), key=lambda t: t[0]) + [(total, e["b"])]
        for (s0, a), (s1, b) in zip(chain, chain[1:]):
            if s1 - s0 < 0.3:
                continue
            shape = slice_poly(pts, s0, s1)
            if len(shape) < 2:
                shape = [(nodes[a]["x"], nodes[a]["y"]), (nodes[b]["x"], nodes[b]["y"])]
            length = plen(shape)
            if length < 0.3:
                continue
            edges.append({"a": a, "b": b, "len_m": round(length * M_PER_PX, 1),
                          "name": e["name"], "cls": e["cls"],
                          "shape": [[round(q[0], 1), round(q[1], 1)] for q in shape]})

    for nid, target in spurs:
        a = nodes[nid]
        b = nodes[target]
        length = math.hypot(a["x"] - b["x"], a["y"] - b["y"])
        edges.append({"a": nid, "b": target,
                      "len_m": round(max(length * M_PER_PX, 0.5), 1),
                      "name": "连接步道", "cls": "link",
                      "shape": [[round(a["x"], 1), round(a["y"], 1)],
                                [round(b["x"], 1), round(b["y"], 1)]]})

    deg = Counter()
    for e in edges:
        deg[e["a"]] += 1
        deg[e["b"]] += 1
    isolated = [n for n in nodes if deg[n] == 0]
    total_m = sum(e["len_m"] for e in edges)
    kinds = Counter(n["kind"] for n in nodes.values())
    print(f"nodes={len(nodes)} {dict(kinds)}")
    print(f"edges={len(edges)} total={total_m:.0f} m")
    print(f"isolated nodes={len(isolated)}: {isolated[:8]}")
    print(f"POI degrees={sorted(deg[f'P{i:02d}'] for i in poi_node)}")

    out = {
        "scale_m_per_px": M_PER_PX,
        "crs": "gcj02-pixel",
        "transform_from_ref_map": {"scale": S, "tx": TX, "ty": TY},
        "nodes": [{"id": k, "x": round(v["x"], 1), "y": round(v["y"], 1), "kind": v["kind"],
                   "name": v.get("name", ""), "type": v.get("type", "")}
                  for k, v in nodes.items()],
        "edges": edges,
        "poi": [{"id": int(p), "node": poi_node[int(p)], "name": v["name"]}
                for p, v in poi.items()],
    }
    json.dump(out, open(os.path.join(OUT, "campus_graph.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)

    im = Image.open(BASE).convert("RGB")
    dr = ImageDraw.Draw(im)
    for e in edges:
        color = (210, 30, 30) if e["cls"] == "arterial" else (
            (30, 90, 210) if e["cls"] == "campus" else (20, 150, 80))
        pts = [tuple(q) for q in e["shape"]]
        if len(pts) >= 2:
            dr.line(pts, fill=color, width=3, joint="curve")
    font = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 20)
    for pid, nid in poi_node.items():
        p = nodes[nid]
        dr.ellipse((p["x"] - 9, p["y"] - 9, p["x"] + 9, p["y"] + 9),
                   fill=(255, 0, 255), outline=(110, 0, 110))
        dr.text((p["x"] + 11, p["y"] - 12), str(pid), fill=(170, 0, 170), font=font)
    im.save(os.path.join(OUT, "campus_graph_check.png"))
    im.resize((im.width // 2, im.height // 2)).save(
        os.path.join(ROOT, "build", "real_map", "campus_graph_check_small.png"))
    print("check ->", os.path.join(OUT, "campus_graph_check.png"))


if __name__ == "__main__":
    main()
