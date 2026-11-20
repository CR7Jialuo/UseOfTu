# -*- coding: utf-8 -*-
"""用两幅图里共有的水面做配准，求“我的地图像素 -> 你的路网像素”的相似变换，并校验四个校门。"""
import math
from collections import deque

import numpy as np
from PIL import Image

MY = "build/template/campus_map.png"
THEIRS = "build/road_data/路网校对图.png"

my = np.asarray(Image.open(MY).convert("RGB")).astype(int)
th = np.asarray(Image.open(THEIRS).convert("RGB")).astype(int)
print("my", my.shape, "theirs", th.shape)

# 我的图右侧有图例栏，裁掉
my_map_w = int(my.shape[1] * 0.735)
my = my[:, :my_map_w]


def water_mask(a):
    R, G, B = a[:, :, 0], a[:, :, 1], a[:, :, 2]
    return (B > 180) & (B - G > 12) & (G - R > 8) & (R < 210)


def largest_blob(mask, min_area=2000):
    ys, xs = np.where(mask)
    print("  water pixels:", len(xs))
    pts = set(zip(xs.tolist(), ys.tolist()))
    seen = set()
    best = None
    blobs = 0
    for p in pts:
        if p in seen:
            continue
        q = deque([p])
        seen.add(p)
        comp = []
        while q:
            cx, cy = q.popleft()
            comp.append((cx, cy))
            for dx in (-2, -1, 0, 1, 2):
                for dy in (-2, -1, 0, 1, 2):
                    n = (cx + dx, cy + dy)
                    if n in pts and n not in seen:
                        seen.add(n)
                        q.append(n)
        if len(comp) < min_area:
            continue
        blobs += 1
        xs2 = [c[0] for c in comp]
        ys2 = [c[1] for c in comp]
        area = len(comp)
        cx = sum(xs2) / area
        cy = sum(ys2) / area
        item = {"area": area, "cx": cx, "cy": cy,
                "bbox": (min(xs2), min(ys2), max(xs2), max(ys2))}
        if best is None or area > best["area"]:
            best = item
    print("  blobs>min:", blobs)
    return best


print("my water:")
mw = largest_blob(water_mask(my))
print("  ", mw)
print("their water:")
tw = largest_blob(water_mask(th))
print("  ", tw)

if mw and tw:
    # 以 bbox 边长比作为尺度（同一水体）
    mw_w = mw["bbox"][2] - mw["bbox"][0]
    mw_h = mw["bbox"][3] - mw["bbox"][1]
    tw_w = tw["bbox"][2] - tw["bbox"][0]
    tw_h = tw["bbox"][3] - tw["bbox"][1]
    s_x = tw_w / mw_w
    s_y = tw_h / mw_h
    print(f"\nscale by bbox: sx={s_x:.4f} sy={s_y:.4f} (area ratio sqrt={math.sqrt(tw['area']/mw['area']):.4f})")
    s = (s_x + s_y) / 2
    tx = tw["cx"] - s * mw["cx"]
    ty = tw["cy"] - s * mw["cy"]
    print(f"transform: their = {s:.4f} * mine + ({tx:.1f}, {ty:.1f})")

    gates = {"1号门": (1122, 1472), "2号门": (632, 1533), "3号门": (1386, 570), "南门": (1526, 1424)}
    import csv
    import os
    nodes = {}
    with open("build/road_data/路网拓扑_nodes.csv", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            nodes[r["node_id"]] = (float(r["x_gcj02"]), float(r["y_gcj02"]))
    print("\ngate check (mapped -> nearest network node):")
    for name, (px, py) in gates.items():
        mx, my2 = s * px + tx, s * py + ty
        best = min(nodes.items(), key=lambda kv: math.hypot(kv[1][0] - mx, kv[1][1] - my2))
        d = math.hypot(best[1][0] - mx, best[1][1] - my2)
        print(f"  {name}: mine({px},{py}) -> theirs({mx:.0f},{my2:.0f}) "
              f"nearest {best[0]}({best[1][0]:.0f},{best[1][1]:.0f}) dist={d:.1f}px = {d*0.492:.1f}m")
