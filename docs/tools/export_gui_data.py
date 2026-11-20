# -*- coding: utf-8 -*-
"""导出 GUI 需要的附加数据：
   data/campus_real/meta.json     标定参数（m/px、像素原点、底图文件）
   data/campus_real/shapes.csv    每条道路的折线形状（米制，"x1 y1;x2 y2;..."）
GUI 读不到 shapes.csv 时退化为直线绘制，核心库完全不受影响。
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, "data", "campus_real")


def main():
    g = json.load(open(os.path.join(DATA, "campus_graph.json"), encoding="utf-8"))
    m = g["scale_m_per_px"]

    # 与 export_campus_csv.py 必须保持一致的取整/原点规则
    xs = [n["x"] for n in g["nodes"]]
    ys = [n["y"] for n in g["nodes"]]
    minx, maxy = min(xs), max(ys)

    def to_m(px, py):
        return ((px - minx) * m, (maxy - py) * m)

    meta = {
        "title": "江苏海洋大学（苍梧校区）",
        "m_per_px": m,
        "px_origin": [minx, maxy],
        "basemap": "basemap.png",
        "basemap_size": [2572, 2582],
    }
    json.dump(meta, open(os.path.join(DATA, "meta.json"), "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)

    # roads.csv 的行序 == campus_graph.json 的边序？不保证，按端点对匹配
    # 导出时用 (a,b) 与 CSV 中同序号的边对应：这里直接按 export_campus_csv 的规则重算
    # 简单做法：shapes.csv 用 "起点,终点" 作为键，GUI 用两端点查表
    # graph_id -> 数值编号（与 spots.csv/roads.csv 一致）
    idmap = {}
    with open(os.path.join(DATA, "node_ids.csv"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            gid, num, _kind = line.strip().split(",")
            idmap[gid] = int(num)

    seen = set()
    rows = []
    for e in g["edges"]:
        key = tuple(sorted((e["a"], e["b"])))
        if key in seen:
            continue
        seen.add(key)
        pts = [to_m(p[0], p[1]) for p in e["shape"]]
        if len(pts) < 2:
            continue
        body = ";".join(f"{x:.1f} {y:.1f}" for x, y in pts)
        rows.append((idmap.get(e["a"], e["a"]), idmap.get(e["b"], e["b"]), body))

    with open(os.path.join(DATA, "shapes.csv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# 道路折线形状（米制）：起点节点,终点节点,x1 y1;x2 y2;...\n")
        f.write("# 节点编号与 spots.csv 一致；GUI 用两端点查折线，查不到则画直线\n")
        for a, b, body in rows:
            f.write(f"{a},{b},{body}\n")

    print(f"meta.json + shapes.csv written ({len(rows)} shapes)")


if __name__ == "__main__":
    main()
