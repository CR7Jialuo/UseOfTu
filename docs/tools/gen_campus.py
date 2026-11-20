#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""校园导航系统 —— 校园图数据生成器。

单一数据源：本脚本中的 SPOTS / ROADS 布局表。
输出：
  data/campus/spots.csv        地点表（UTF-8）
  data/campus/roads.csv        道路表（UTF-8）
  src/default_campus.c         内置默认数据集（内置兜底 + 单元测试对照）

坐标单位为米，原点在西南角；道路长度按直线距离乘路线系数 ROAD_FACTOR 估算，
步行时间按 78 米/分钟（约 4.7 km/h）估算。数值为教学示意值，非实测测绘数据。

用法：
  python gen_campus.py [项目根目录]
"""

import math
import os
import sys

ROAD_FACTOR = 1.12          # 道路绕行系数
WALK_SPEED_M_PER_MIN = 78.0  # 步行速度
CAMPUS_TITLE = "江苏海洋大学（苍梧校区）"

SPOT_TYPE_ENUM = {
    "gate": "SPOT_GATE",
    "teaching": "SPOT_TEACHING",
    "library": "SPOT_LIBRARY",
    "admin": "SPOT_ADMIN",
    "dorm": "SPOT_DORM",
    "canteen": "SPOT_CANTEEN",
    "sport": "SPOT_SPORT",
    "medical": "SPOT_MEDICAL",
    "service": "SPOT_SERVICE",
    "scenic": "SPOT_SCENIC",
}

# id, 名称, 类型, x, y, 简介
SPOTS = [
    (1,  "东大门",     "gate",     900, 360, "学校正门 临近公交站与校外主干道"),
    (2,  "南大门",     "gate",     460,  20, "南侧出入口 靠近学生生活区"),
    (3,  "北门",       "gate",     430, 700, "北侧出入口 通往运动场方向"),
    (4,  "图书馆",     "library",  460, 350, "藏书与自习中心 校园地理中心"),
    (5,  "行政楼",     "admin",    360, 410, "校务办公与一站式办事大厅"),
    (6,  "第一教学楼", "teaching", 300, 520, "公共基础课教学楼"),
    (7,  "第二教学楼", "teaching", 395, 575, "专业课教学楼"),
    (8,  "第三教学楼", "teaching", 500, 548, "多媒体教室集中楼"),
    (9,  "实验楼",     "teaching", 600, 480, "计算机与电子类实验室"),
    (10, "海洋楼",     "teaching", 695, 560, "海洋工程与水产学科楼"),
    (11, "信息楼",     "teaching", 545, 610, "信息技术与人工智能实验室"),
    (12, "体育馆",     "sport",    770, 340, "篮球馆与室内运动场"),
    (13, "田径场",     "sport",    715, 190, "400 米标准跑道与看台"),
    (14, "学生一食堂", "canteen",  270, 215, "主食堂 共三层"),
    (15, "学生二食堂", "canteen",  600, 235, "风味餐厅与清真窗口"),
    (16, "宿舍A区",    "dorm",     170, 130, "本科生宿舍 1 至 8 号楼"),
    (17, "宿舍B区",    "dorm",     665, 110, "本科生宿舍 9 至 16 号楼"),
    (18, "校医院",     "medical",  860, 120, "24 小时门诊与急救室"),
    (19, "快递服务中心", "service", 880, 470, "菜鸟驿站与快递收发"),
    (20, "人工湖",     "scenic",   470, 270, "静湖与环湖步道"),
    (21, "创新创业园", "service",  140, 425, "创业孵化与工程实训基地"),
    (22, "校训广场",   "scenic",   400, 300, "校训石与升旗广场"),
    (23, "观海亭",     "scenic",   890, 650, "东北角观景平台 仅一条步道可达"),
    (24, "实训码头",   "service",   95, 640, "海洋调查实训基地 单通路出入"),
]

# 无向道路 (a, b)
ROADS = [
    (1, 12), (1, 19), (1, 18),
    (2, 14), (2, 20), (2, 5),
    (3, 7), (3, 11), (3, 6),
    (4, 20), (4, 5), (4, 12), (4, 9), (4, 22),
    (5, 14), (5, 6), (5, 21), (5, 22),
    (6, 7), (6, 21),
    (7, 8), (7, 11),
    (8, 9), (8, 11),
    (9, 10), (9, 19),
    (10, 11),
    (12, 13),
    (13, 15), (13, 17), (13, 18),
    (14, 16), (14, 22),
    (15, 17), (15, 4), (15, 20),
    (16, 21),
    (17, 18),
    (20, 22),
    (19, 23),
    (21, 24),
]


def spot_by_id(sid):
    for s in SPOTS:
        if s[0] == sid:
            return s
    raise KeyError(sid)


def road_length(a, b):
    xa, ya = spot_by_id(a)[3], spot_by_id(a)[4]
    xb, yb = spot_by_id(b)[3], spot_by_id(b)[4]
    return round(math.hypot(xb - xa, yb - ya) * ROAD_FACTOR, 1)


def walk_minutes(length):
    return round(length / WALK_SPEED_M_PER_MIN, 1)


def build_roads():
    out = []
    for idx, (a, b) in enumerate(ROADS, start=1):
        length = road_length(a, b)
        if length < 1.0:
            raise ValueError("道路长度异常: %d-%d" % (a, b))
        out.append({
            "id": idx,
            "from": a,
            "to": b,
            "len": length,
            "min": walk_minutes(length),
        })
    return out


def check_consistency():
    ids = [s[0] for s in SPOTS]
    if ids != list(range(1, len(SPOTS) + 1)):
        raise ValueError("地点编号必须从 1 连续递增")
    seen = set()
    for a, b in ROADS:
        if a == b:
            raise ValueError("自环道路: %d" % a)
        key = (min(a, b), max(a, b))
        if key in seen:
            raise ValueError("重复道路: %d-%d" % key)
        seen.add(key)
        spot_by_id(a)
        spot_by_id(b)
    # 连通性检查（BFS）
    adj = {s[0]: set() for s in SPOTS}
    for a, b in ROADS:
        adj[a].add(b)
        adj[b].add(a)
    start = SPOTS[0][0]
    stack, seen_nodes = [start], {start}
    while stack:
        cur = stack.pop()
        for nxt in adj[cur]:
            if nxt not in seen_nodes:
                seen_nodes.add(nxt)
                stack.append(nxt)
    if len(seen_nodes) != len(SPOTS):
        missing = sorted(set(ids) - seen_nodes)
        raise ValueError("图不连通，孤立地点: %s" % missing)


def write_csv(root, roads):
    data_dir = os.path.join(root, "data", "campus")
    os.makedirs(data_dir, exist_ok=True)

    spots_path = os.path.join(data_dir, "spots.csv")
    with open(spots_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# title: %s\n" % CAMPUS_TITLE)
        f.write("# 江苏海洋大学（苍梧校区）校园导航地点表\n")
        f.write("# 字段：编号,名称,类型,横坐标(米),纵坐标(米),简介\n")
        f.write("# 坐标为教学示意值，原点在校园西南角，非实测测绘数据\n")
        for sid, name, stype, x, y, brief in SPOTS:
            f.write("%d,%s,%s,%d,%d,%s\n" % (sid, name, stype, x, y, brief))

    roads_path = os.path.join(data_dir, "roads.csv")
    with open(roads_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# 江苏海洋大学（苍梧校区）校园道路表\n")
        f.write("# 字段：编号,起点编号,终点编号,长度(米),步行时间(分钟)\n")
        f.write("# 无向道路，长度按路线系数 %.2f 与 %.1f 米/分钟步行速度估算\n"
                % (ROAD_FACTOR, WALK_SPEED_M_PER_MIN))
        for r in roads:
            f.write("%d,%d,%d,%.1f,%.1f\n" % (r["id"], r["from"], r["to"], r["len"], r["min"]))
    return spots_path, roads_path


def write_c_source(root, roads):
    src_dir = os.path.join(root, "src")
    os.makedirs(src_dir, exist_ok=True)
    path = os.path.join(src_dir, "default_campus.c")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* 本文件由 docs/tools/gen_campus.py 自动生成，请勿手工修改。\n")
        f.write(" * 数据源：脚本内的 SPOTS / ROADS 布局表。\n")
        f.write(" * 作用：内置默认校园数据集（CSV 缺失时的兜底数据，同时用于一致性测试）。\n */\n\n")
        f.write('#include "campus.h"\n\n')
        f.write('#include <string.h>\n\n')
        f.write("static const Spot k_default_spots[] = {\n")
        for sid, name, stype, x, y, brief in SPOTS:
            f.write('    { %d, "%s", %s, %.1f, %.1f, "%s" },\n'
                    % (sid, name, SPOT_TYPE_ENUM[stype], float(x), float(y), brief))
        f.write("};\n\n")
        f.write("static const Road k_default_roads[] = {\n")
        for r in roads:
            f.write('    { %d, %d, %d, %.1f, %.1f, 1, "" },\n'
                    % (r["id"], r["from"], r["to"], r["len"], r["min"]))
        f.write("};\n\n")
        f.write("int campus_load_default(Campus *c)\n{\n")
        f.write("    int i;\n")
        f.write("    if (c == NULL) {\n        return -1;\n    }\n")
        f.write("    memset(c, 0, sizeof(*c));\n")
        f.write('    strcpy(c->title, "%s");\n\n' % CAMPUS_TITLE)
        f.write("    c->width_m = %.1f;\n" % (max(s[3] for s in SPOTS) + 30.0))
        f.write("    c->height_m = %.1f;\n" % (max(s[4] for s in SPOTS) + 30.0))
        f.write("    c->n_spots = (int)(sizeof(k_default_spots) / sizeof(k_default_spots[0]));\n")
        f.write("    c->n_roads = (int)(sizeof(k_default_roads) / sizeof(k_default_roads[0]));\n")
        f.write("    for (i = 0; i < c->n_spots; ++i) {\n        c->spots[i] = k_default_spots[i];\n    }\n")
        f.write("    for (i = 0; i < c->n_roads; ++i) {\n        c->roads[i] = k_default_roads[i];\n    }\n")
        f.write("    return 0;\n}\n")


def main(argv):
    root = argv[1] if len(argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    check_consistency()
    roads = build_roads()
    spots_path, roads_path = write_csv(root, roads)
    write_c_source(root, roads)
    total = sum(r["len"] for r in roads)
    print("地点 %d 个，道路 %d 条，道路总长 %.1f 米" % (len(SPOTS), len(roads), total))
    print("已生成: %s" % spots_path)
    print("已生成: %s" % roads_path)
    print("已生成: %s" % os.path.join(root, "src", "default_campus.c"))


if __name__ == "__main__":
    main(sys.argv)
