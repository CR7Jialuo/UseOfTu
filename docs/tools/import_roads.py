#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把外部道路矢量（KML / GPX / JSON / 图上描线的彩色像素）转成校园道路折线（像素坐标）。

支持三种输入：
  1) KML：<Placemark><name>路名</name><LineString><coordinates>lon,lat,0 ...</coordinates>
  2) GPX：<trk><name>路名</name><trkseg><trkpt lat="" lon=""/>...
  3) JSON：{"roads":[{"name":"香海路","type":"main","points_px":[[x,y],...]}]}
     或 {"roads":[{"name":"香海路","type":"main","points":[[lon,lat],...]}]}

经纬度 -> 像素：以“校区中心像素 <-> 校区中心经纬度”为锚点，按 0.735 m/px 的标定做
等距圆柱近似。经纬度可按 --datum 指定坐标系：
  gcj02（默认，高德/腾讯地图取点）  wgs84（手机 GPS、Google Earth，先纠偏到 GCJ-02）

用法：
  python import_roads.py roads.kml --datum gcj02
  python import_roads.py roads.gpx --datum wgs84 --out data/campus_real/roads_source.json
"""

import argparse
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

M_PER_PX = 200.0 / 272.0
CENTER_LL = (119.2200, 34.6074)     # 底图标注的校区坐标
CENTER_PX = (869.0, 885.0)          # 校区几何中心像素（由 20 个 POI 包围盒估得）
METERS_PER_DEG_LAT = 110574.0


def gcj02_from_wgs84(lon, lat):
    """WGS84 -> GCJ-02（国测局偏移），用于把手机 GPS 轨迹对齐到高德底图。"""
    a = 6378245.0
    ee = 0.00669342162296594323

    def transform_lat(x, y):
        ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * math.sqrt(abs(x))
        ret += (20.0 * math.sin(6.0 * x * math.pi) + 20.0 * math.sin(2.0 * x * math.pi)) * 2.0 / 3.0
        ret += (20.0 * math.sin(y * math.pi) + 40.0 * math.sin(y / 3.0 * math.pi)) * 2.0 / 3.0
        ret += (160.0 * math.sin(y / 12.0 * math.pi) + 320.0 * math.sin(y * math.pi / 30.0)) * 2.0 / 3.0
        return ret

    def transform_lon(x, y):
        ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * math.sqrt(abs(x))
        ret += (20.0 * math.sin(6.0 * x * math.pi) + 20.0 * math.sin(2.0 * x * math.pi)) * 2.0 / 3.0
        ret += (20.0 * math.sin(x * math.pi) + 40.0 * math.sin(x / 3.0 * math.pi)) * 2.0 / 3.0
        ret += (150.0 * math.sin(x / 12.0 * math.pi) + 300.0 * math.sin(x / 30.0 * math.pi)) * 2.0 / 3.0
        return ret

    dlat = transform_lat(lon - 105.0, lat - 35.0)
    dlon = transform_lon(lon - 105.0, lat - 35.0)
    radlat = lat / 180.0 * math.pi
    magic = math.sin(radlat)
    magic = 1 - ee * magic * magic
    sqrtmagic = math.sqrt(magic)
    dlat = (dlat * 180.0) / ((a * (1 - ee)) / (magic * sqrtmagic) * math.pi)
    dlon = (dlon * 180.0) / (a / sqrtmagic * math.cos(radlat) * math.pi)
    return lon + dlon, lat + dlat


def ll_to_px(lon, lat, datum):
    if datum == "wgs84":
        lon, lat = gcj02_from_wgs84(lon, lat)
    clon, clat = CENTER_LL
    m_per_deg_lon = METERS_PER_DEG_LAT * math.cos(math.radians(clat))
    dx = (lon - clon) * m_per_deg_lon / M_PER_PX
    dy = (lat - clat) * METERS_PER_DEG_LAT / M_PER_PX
    return [CENTER_PX[0] + dx, CENTER_PX[1] - dy]


def parse_kml(path):
    tree = ET.parse(path)
    root = tree.getroot()
    roads = []
    for pm in root.iter():
        if pm.tag.split("}")[-1] != "Placemark":
            continue
        name = ""
        coords_text = ""
        for child in pm.iter():
            tag = child.tag.split("}")[-1]
            if tag == "name" and not name:
                name = (child.text or "").strip()
            elif tag == "coordinates" and not coords_text:
                coords_text = (child.text or "").strip()
        if not coords_text:
            continue
        pts = []
        for chunk in coords_text.replace("\n", " ").split():
            parts = chunk.split(",")
            if len(parts) >= 2:
                try:
                    pts.append((float(parts[0]), float(parts[1])))
                except ValueError:
                    continue
        if len(pts) >= 2:
            roads.append({"name": name or "未命名道路", "points": pts})
    return roads


def parse_gpx(path):
    tree = ET.parse(path)
    root = tree.getroot()
    roads = []
    for trk in root.iter():
        if trk.tag.split("}")[-1] != "trk":
            continue
        name = ""
        pts = []
        for child in trk:
            tag = child.tag.split("}")[-1]
            if tag == "name":
                name = (child.text or "").strip()
            elif tag == "trkseg":
                for pt in child:
                    if pt.tag.split("}")[-1] != "trkpt":
                        continue
                    try:
                        pts.append((float(pt.get("lon")), float(pt.get("lat"))))
                    except (TypeError, ValueError):
                        continue
        if len(pts) >= 2:
            roads.append({"name": name or "未命名道路", "points": pts})
    return roads


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--datum", choices=["gcj02", "wgs84"], default="gcj02")
    ap.add_argument("--out", default="data/campus_real/roads_source.json")
    ap.add_argument("--default-type", default="branch")
    ap.add_argument("--main-name", action="append", default=[],
                    help="按路名关键字把道路标为主干道，可多次指定")
    args = ap.parse_args(argv[1:])

    ext = os.path.splitext(args.source)[1].lower()
    if ext == ".kml":
        raw = parse_kml(args.source)
    elif ext == ".gpx":
        raw = parse_gpx(args.source)
    elif ext == ".json":
        data = json.load(open(args.source, encoding="utf-8"))
        raw = data.get("roads", [])
    else:
        print("unsupported:", ext)
        return 2

    roads = []
    for r in raw:
        if "points_px" in r:
            pts_px = r["points_px"]
        else:
            pts_px = [ll_to_px(lon, lat, args.datum) for lon, lat in r["points"]]
        name = r.get("name", "未命名道路")
        rtype = r.get("type") or (
            "main" if any(k in name for k in args.main_name) else args.default_type)
        roads.append({"name": name, "type": rtype,
                      "points_px": [[round(x, 1), round(y, 1)] for x, y in pts_px]})

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    json.dump({"m_per_px": M_PER_PX, "datum": args.datum, "roads": roads},
              open(args.out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    total = sum(len(r["points_px"]) for r in roads)
    print(f"imported {len(roads)} roads, {total} vertices -> {args.out}")
    for r in roads[:8]:
        print(f"  {r['type']:6s} {r['name']:<10} {len(r['points_px'])} pts")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
