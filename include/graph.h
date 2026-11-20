/* graph.h —— 邻接表（CSR）图结构。
 *
 * 校园道路是无向带权图：一条道路对应两条有向弧，权重可取长度或步行时间。
 * 采用 CSR（压缩稀疏行）存储，便于缓存友好的遍历；Floyd 需要矩阵，
 * 由 graph_adj_matrix 按需构建。
 */
#ifndef GRAPH_H
#define GRAPH_H

#include "nav.h"
#include "campus.h"

typedef struct {
    int     n;        /* 点数，1..n */
    int    *off;      /* 长度 n+2 的 CSR 偏移 */
    int    *to;       /* 弧终点 */
    double *w;        /* 弧权重 */
    int    *road;     /* 弧对应道路编号 */
    int     n_arcs;

    int     n_edges;  /* 参与建图的道路条数（封闭道路不计） */
    int    *eu;       /* 边表：起点 */
    int    *ev;       /* 边表：终点 */
    double *ew;       /* 边表：权重 */
    int    *eid;      /* 边表：道路编号 */

    int     by_time;  /* 1 表示权重为步行时间，0 表示长度 */
} Graph;

/* 由校园数据建图。by_time 选择权重口径。封闭道路被跳过。 */
int  graph_build(const Campus *c, int by_time, Graph *g, char *err, size_t cap);
void graph_free(Graph *g);

/* 构建 n*n 邻接矩阵（调用方 free）。不可达处填 NAV_INF，对角线为 0。 */
int  graph_adj_matrix(const Graph *g, double **mat, char *err, size_t cap);

/* 边权重口径的文本名（"距离"/"时间"）。 */
const char *graph_weight_name(int by_time);

#endif /* GRAPH_H */
