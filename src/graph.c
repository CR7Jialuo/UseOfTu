/* graph.c —— 由校园数据构建 CSR 邻接表，以及按需构建邻接矩阵。 */
#include "graph.h"
#include "err.h"

#include <stdlib.h>
#include <string.h>

const char *graph_weight_name(int by_time)
{
    return by_time ? "步行时间" : "距离";
}

int graph_build(const Campus *c, int by_time, Graph *g, char *err, size_t cap)
{
    int i, n, n_edges, n_arcs;
    int *deg = NULL;
    int *cursor = NULL;

    if (c == NULL || g == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    n = c->n_spots;
    if (n < 1) {
        nav_err_set(err, cap, "地点数为 0，无法建图");
        return NAV_ERR_FMT;
    }
    memset(g, 0, sizeof(*g));
    g->n = n;
    g->by_time = by_time ? 1 : 0;

    n_edges = 0;
    for (i = 0; i < c->n_roads; ++i) {
        if (c->roads[i].open) {
            ++n_edges;
        }
    }
    n_arcs = n_edges * 2;

    g->off = (int *)calloc((size_t)n + 2, sizeof(int));
    g->to = (int *)calloc((size_t)(n_arcs > 0 ? n_arcs : 1), sizeof(int));
    g->w = (double *)calloc((size_t)(n_arcs > 0 ? n_arcs : 1), sizeof(double));
    g->road = (int *)calloc((size_t)(n_arcs > 0 ? n_arcs : 1), sizeof(int));
    g->eu = (int *)calloc((size_t)(n_edges > 0 ? n_edges : 1), sizeof(int));
    g->ev = (int *)calloc((size_t)(n_edges > 0 ? n_edges : 1), sizeof(int));
    g->ew = (double *)calloc((size_t)(n_edges > 0 ? n_edges : 1), sizeof(double));
    g->eid = (int *)calloc((size_t)(n_edges > 0 ? n_edges : 1), sizeof(int));
    deg = (int *)calloc((size_t)n + 2, sizeof(int));
    cursor = (int *)calloc((size_t)n + 2, sizeof(int));

    if (g->off == NULL || g->to == NULL || g->w == NULL || g->road == NULL || g->eu == NULL ||
        g->ev == NULL || g->ew == NULL || g->eid == NULL || deg == NULL || cursor == NULL) {
        free(deg);
        free(cursor);
        graph_free(g);
        nav_err_set(err, cap, "内存不足（图：%d 点 / %d 弧）", n, n_arcs);
        return NAV_ERR_NOMEM;
    }

    /* 第一遍：统计度数。 */
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        if (!r->open) {
            continue;
        }
        ++deg[r->from];
        ++deg[r->to];
    }
    g->off[0] = 0;
    for (i = 1; i <= n; ++i) {
        g->off[i] = g->off[i - 1] + deg[i];
    }
    /* 约定：节点 i（1-based）的弧区间为 [off[i-1], off[i])。 */

    /* 第二遍：填充弧与边表。 */
    g->n_arcs = n_arcs;
    g->n_edges = n_edges;
    {
        int edge_idx = 0;
        for (i = 1; i <= n; ++i) {
            cursor[i] = g->off[i - 1];
        }
        for (i = 0; i < c->n_roads; ++i) {
            const Road *r = &c->roads[i];
            double w;
            int pos_from, pos_to;
            if (!r->open) {
                continue;
            }
            w = by_time ? r->walk_min : r->len_m;
            pos_from = cursor[r->from]++;
            pos_to = cursor[r->to]++;
            g->to[pos_from] = r->to;
            g->w[pos_from] = w;
            g->road[pos_from] = r->id;
            g->to[pos_to] = r->from;
            g->w[pos_to] = w;
            g->road[pos_to] = r->id;
            g->eu[edge_idx] = r->from;
            g->ev[edge_idx] = r->to;
            g->ew[edge_idx] = w;
            g->eid[edge_idx] = r->id;
            ++edge_idx;
        }
    }
    free(deg);
    free(cursor);
    return NAV_OK;
}

void graph_free(Graph *g)
{
    if (g == NULL) {
        return;
    }
    free(g->off);
    free(g->to);
    free(g->w);
    free(g->road);
    free(g->eu);
    free(g->ev);
    free(g->ew);
    free(g->eid);
    memset(g, 0, sizeof(*g));
}

int graph_adj_matrix(const Graph *g, double **mat, char *err, size_t cap)
{
    double *m;
    int i, j, n;

    if (g == NULL || mat == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    n = g->n;
    m = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (m == NULL) {
        nav_err_set(err, cap, "内存不足（邻接矩阵 %d x %d）", n, n);
        return NAV_ERR_NOMEM;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            m[(size_t)i * (size_t)n + (size_t)j] = NAV_INF;
        }
        m[(size_t)i * (size_t)n + (size_t)i] = 0.0;
    }
    for (i = 0; i < n; ++i) {
        int arc;
        for (arc = g->off[i]; arc < g->off[i + 1]; ++arc) {
            int to = g->to[arc];
            double w = g->w[arc];
            double *slot = &m[(size_t)i * (size_t)n + (size_t)(to - 1)];
            if (w < *slot) {
                *slot = w;
            }
        }
    }
    *mat = m;
    return NAV_OK;
}
