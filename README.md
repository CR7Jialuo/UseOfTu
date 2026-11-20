# 校园导航系统（图的最短路径）

> 数据结构课程设计 · C11 · 江苏海洋大学（苍梧校区）示意校园

一个用**无向带权图**建模校园路网的命令行导航程序：支持两点最短路、多点巡游、最近设施、全源最短路、关键道路分析、最小生成树，以及图的增删改与持久化。

## 功能一览

| 编号 | 功能 | 算法/说明 |
|---|---|---|
| 1 | 校园示意地图 | 按坐标把 24 个地点与 41 条道路画成 ASCII 图 |
| 2 | 地点列表 / 详情 | 类型、坐标、简介、相邻道路 |
| 3 | 路径规划 | Dijkstra（二叉堆，O((V+E)logV)），可按距离或步行时间 |
| 4 | 多点巡游 | Held-Karp 精确解（途经点 ≤ 12）+ 最近邻 2-opt 近似（> 12） |
| 5 | 最近设施 | 单源 Dijkstra + 同类设施筛选 |
| 6 | 全源最短路 | Floyd-Warshall，O(V³)，输出矩阵并重建路径 |
| 7 | 关键道路分析 | Tarjan 求割点与桥，评估封路风险 |
| 8 | 最小生成树 | Prim 与 Kruskal 双实现互相校验 |
| 9 | 图编辑 | 增删地点/道路、改长度与时间、封闭/恢复道路 |
| 10 | 数据管理 | CSV 导入导出、二进制快照（magic + 版本 + CRC32） |
| 11 | 算法对比实验 | 随机图基准，输出各算法耗时 |

## 快速开始

```bash
# Windows（脚本自带找 gcc，找不到会提示设置 CC）
build.cmd          # 编译 -> build\nav.exe
test.cmd           # 单元测试（1500+ 断言）
run.cmd            # 交互菜单

# 或者直接用 PowerShell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify.ps1
```

```bash
# Linux / CI
make && make test && make e2e
```

运行示例：

```bash
nav route 宿舍A区 图书馆 --json
nav route 东大门 图书馆 --by time
nav tour 东大门 图书馆 第一教学楼 学生一食堂
nav near 宿舍A区 canteen
nav critical
nav bench --nodes 120 --repeat 50
```

## 目录结构

```text
campus-nav/
├─ include/          头文件（nav/campus/graph/algo/app/ui/platform/crc32/bench/err）
├─ src/              实现（含自动生成的 default_campus.c）
├─ tests/            自动化测试（独立编译，直接链接核心 .c）
├─ data/campus/      校园数据：spots.csv + roads.csv
├─ docs/             报告、PPT、图表、生成脚本（gen_campus.py / make_docs.py ...）
├─ scripts/          build/test/run/e2e/verify/export/push
├─ .github/workflows/ci.yml
├─ Makefile  build.cmd  test.cmd  run.cmd
└─ README.md
```

## 数据格式

`data/campus/spots.csv`：

```csv
# title: 江苏海洋大学（苍梧校区）
# 字段：编号,名称,类型,横坐标(米),纵坐标(米),简介
1,东大门,gate,900,360,学校正门 临近公交站与校外主干道
```

`data/campus/roads.csv`（无向边）：

```csv
# 字段：编号,起点编号,终点编号,长度(米),步行时间(分钟)
1,1,12,147.3,1.9
```

类型取值为 `gate/teaching/library/admin/dorm/canteen/sport/medical/service/scenic`。
数据由 `docs/tools/gen_campus.py` 生成（坐标 × 路线系数 1.12 得到长度，按 78 米/分钟折算时间），
`src/default_campus.c` 是同一份数据的内置副本，用于 CSV 缺失时兜底。

> **数据说明**：校园地点的**名称与功能**取自学校实际楼宇类型，但**坐标为教学示意值**，并非实测测绘数据；
> 距离按校园路网绕行系数估算。若需接入真实地图，替换两个 CSV 即可，核心算法无需改动。

## 算法与复杂度

| 算法 | 复杂度 | 适用场景 |
|---|---|---|
| Dijkstra（二叉堆） | O((V+E)logV) | 单源、非负权、交互查询 |
| Floyd-Warshall | O(V³) | 全源矩阵、小规模（V ≤ 128） |
| Bellman-Ford | O(VE) | 支持负权、检测负权回路 |
| BFS | O(V+E) | 最少路段数 |
| Tarjan | O(V+E) | 割点、桥（关键道路） |
| Prim | O(V²+E) | 稠密图最小生成树 |
| Kruskal | O(ElogE) | 稀疏图最小生成树 |
| Held-Karp | O(2^k·k²) | 途经点 ≤ 12 的精确巡游 |

## 文档

- 报告：`docs/课程设计报告.docx` / `.pdf`（仓库内为脱敏版，个人信息处为【请填写】）
- 答辩：`docs/答辩演示.pptx`（12 页）
- 插图：`docs/diagrams/`；生成脚本：`docs/tools/`（Pillow + python-docx + python-pptx）
- 一键重建：`scripts/make_docs.ps1`（图片 → DOCX → PDF → 回填目录页码 → 再导出 PDF → PPTX）
- 带真实个人信息的最终版在 `dist/final/`（不提交），由 `scripts/export_submission.ps1` 覆盖进提交包

## 测试

`tests/test_main.c` 覆盖：

- 工具层：CRC32 标准向量、UTF-8 显示宽度
- 数据集：内置数据一致性、CSV 往返与 6 类错误路径、快照往返与篡改/截断检测、编辑操作与重排
- 算法：链/环/星形/随机图上的最短路、负权与负权回路、连通分量、割点与桥、MST 双算法一致、巡游与暴力枚举对比
- 交叉验证：30 组随机图，Dijkstra / Floyd / Bellman-Ford 全点对结果一致
- 性能冒烟：128 节点 Dijkstra 与 Floyd 的时间上限

`scripts/e2e.sh|ps1` 再从命令行角度验证 14 个场景（含 JSON 输出、退出码、导出数据与仓库一致性）。

## 已知限制

- 只有一个校区、24 个地点；更大规模需调整 `NAV_MAX_SPOTS`（矩阵与数组按此静态分配）。
- 巡游问题在途经点 > 12 时用启发式，不保证最优。
- 地点删除会重排编号，外部脚本若缓存编号需要重新读取。

## 许可与致谢

课程设计作业，代码独立实现；图论结论与算法伪代码参考《数据结构（C 语言版）》教材与经典文献，见报告"参考文献"。
