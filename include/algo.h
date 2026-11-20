/* algo.h —— 图算法接口：最短路、遍历、关键边、最小生成树、多点巡游、最近设施。
 *
 * 命名约定：
 *   0            成功
 *   NAV_ERR_*    负值错误码
 *   Path.count==0 且 weight==NAV_INF 表示不连通
 */
#ifndef ALGO_H
#define ALGO_H

#include "nav.h"
#include "graph.h"

/* 一条路径：节点序列 + 总权重 + 算法统计量。 */
typedef struct {
    int    nodes[NAV_MAX_SPOTS];
    int    count;      /* 节点个数 */
    double weight;     /* 路径总权重（长度或时间，取决于建图口径） */
    long   settled;    /* 已确定节点数（Dijkstra 出堆次数） */
    long   relaxed;    /* 松弛次数 */
} Path;

void path_clear(Path *p);

/* 单源最短路（二叉堆优化 Dijkstra，非负权）。
 * dst<=0 表示只求到 src 的距离，不重建路径。 */
int algo_dijkstra(const Graph *g, int src, int dst, Path *out, char *err, size_t cap);

/* 单源最短距离表（内部接口，供最近设施与基准测试复用）。 */
int algo_dijkstra_table(const Graph *g, int src, double *dist, int *prev,
                        long *settled, long *relaxed, char *err, size_t cap);

/* Floyd-Warshall 全源最短路，返回 n*n 的 dist 与 next 矩阵（调用方 free）。 */
int algo_floyd(const Graph *g, double **dist, int **next, char *err, size_t cap);
int algo_floyd_path(const Graph *g, const double *dist, const int *next,
                    int src, int dst, Path *out);

/* Bellman-Ford：支持负权，可检测负权回路。
 * dist/prev 由调用方提供，长度 n+1。返回 NAV_ERR_NEGCYCLE 表示存在负权回路。 */
int algo_bellman(const Graph *g, int src, double *dist, int *prev, char *err, size_t cap);

/* BFS：边权相同（最少路段数）意义下的最短路。 */
int algo_bfs(const Graph *g, int src, int dst, Path *out, char *err, size_t cap);

/* 连通分量标记，comp[1..n] 存 1..k 的标号，返回分量个数。 */
int algo_components(const Graph *g, int *comp, char *err, size_t cap);

/* Tarjan：割点与桥（桥以道路编号返回）。is_ap 长度 n+1。 */
int algo_critical(const Graph *g, int *is_ap, int *bridge_roads, int *n_bridges,
                  char *err, size_t cap);

/* 最小生成树：返回总权重；图不连通返回 -1。
 * eu/ev 长度 n-1，输出选中的边端点；cnt 输出边数。 */
double algo_prim(const Graph *g, int *eu, int *ev, int *cnt);
double algo_kruskal(const Graph *g, int *eu, int *ev, int *cnt);

/* 多点巡游：从 start 出发，经过 via 中每个地点恰好一次，最后到达 end。
 * via 顺序由算法优化（k<=12 用 Held-Karp 精确解，k>12 用最近邻 + 2-opt）。
 * order 输出访问顺序（长度为 k，存地点编号）。 */
int algo_tsp(const Graph *g, int start, const int *via, int k, int end,
             int *order, Path *out, char *err, size_t cap);

/* 最近设施：从 from 出发，找同类设施中加权距离最近的一个。 */
int algo_nearest_type(const Graph *g, const Campus *c, int from, SpotType type,
                      int *best_spot, Path *out, char *err, size_t cap);

#endif /* ALGO_H */
